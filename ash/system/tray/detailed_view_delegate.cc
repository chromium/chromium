// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/detailed_view_delegate.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/icon_button.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

using ContentLayerType = AshColorProvider::ContentLayerType;

namespace {

void ConfigureTitleTriView(TriView* tri_view, TriView::Container container) {
  std::unique_ptr<views::BoxLayout> layout;

  switch (container) {
    case TriView::Container::START:
    case TriView::Container::END: {
      const int left_padding = container == TriView::Container::START
                                   ? kUnifiedBackButtonLeftPadding
                                   : 0;
      layout = std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::TLBR(0, left_padding, 0, 0), kUnifiedTopShortcutSpacing);
      layout->set_main_axis_alignment(
          views::BoxLayout::MainAxisAlignment::kCenter);
      layout->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kCenter);
      break;
    }
    case TriView::Container::CENTER:
      tri_view->SetFlexForContainer(TriView::Container::CENTER, 1.f);

      layout = std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical);
      layout->set_main_axis_alignment(
          views::BoxLayout::MainAxisAlignment::kCenter);
      layout->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kStretch);
      break;
  }

  tri_view->SetContainerLayout(container, std::move(layout));
  tri_view->SetMinSize(container,
                       gfx::Size(0, kUnifiedDetailedViewTitleRowHeight));
}

class BackButton : public IconButton {
 public:
  BackButton(views::Button::PressedCallback callback)
      : IconButton(std::move(callback),
                   IconButton::Type::kSmallFloating,
                   &kUnifiedMenuExpandIcon,
                   IDS_ASH_STATUS_TRAY_PREVIOUS_MENU) {}
  BackButton(const BackButton&) = delete;
  BackButton& operator=(const BackButton&) = delete;
  ~BackButton() override = default;

  // Use the same icon as CollapseButton with rotation.
  void PaintButtonContents(gfx::Canvas* canvas) override {
    gfx::ScopedCanvas scoped(canvas);
    canvas->Translate(gfx::Vector2d(size().width() / 2, size().height() / 2));
    canvas->sk_canvas()->rotate(-90);
    gfx::ImageSkia image = GetImageToPaint();
    canvas->DrawImageInt(image, -image.width() / 2, -image.height() / 2);
  }
};

}  // namespace

DetailedViewDelegate::DetailedViewDelegate(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {}

DetailedViewDelegate::~DetailedViewDelegate() = default;

void DetailedViewDelegate::TransitionToMainView(bool restore_focus) {
  tray_controller_->TransitionToMainView(restore_focus);
}

void DetailedViewDelegate::CloseBubble() {
  tray_controller_->CloseBubble();
}

absl::optional<SkColor> DetailedViewDelegate::GetBackgroundColor() {
  return absl::nullopt;
}

bool DetailedViewDelegate::IsOverflowIndicatorEnabled() const {
  return false;
}

TriView* DetailedViewDelegate::CreateTitleRow(int string_id) {
  auto* tri_view = new TriView(kUnifiedTopShortcutSpacing);

  ConfigureTitleTriView(tri_view, TriView::Container::START);
  ConfigureTitleTriView(tri_view, TriView::Container::CENTER);
  ConfigureTitleTriView(tri_view, TriView::Container::END);

  title_label_ = TrayPopupUtils::CreateDefaultLabel();
  title_label_->SetText(l10n_util::GetStringUTF16(string_id));
  title_label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  TrayPopupUtils::SetLabelFontList(title_label_,
                                   TrayPopupUtils::FontStyle::kTitle);
  tri_view->AddView(TriView::Container::CENTER, title_label_);
  tri_view->SetContainerVisible(TriView::Container::END, false);
  tri_view->SetBorder(
      views::CreateEmptyBorder(kUnifiedDetailedViewTitlePadding));

  return tri_view;
}

views::View* DetailedViewDelegate::CreateTitleSeparator() {
  title_separator_ = new views::Separator();
  title_separator_->SetColorId(ui::kColorAshSystemUIMenuSeparator);
  title_separator_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kTitleRowProgressBarHeight - views::Separator::kThickness, 0, 0, 0)));
  return title_separator_;
}

void DetailedViewDelegate::ShowStickyHeaderSeparator(views::View* view,
                                                     bool show_separator) {
  if (show_separator) {
    view->SetBorder(views::CreatePaddedBorder(
        views::CreateSolidSidedBorder(
            gfx::Insets::TLBR(0, 0, kTraySeparatorWidth, 0),
            AshColorProvider::Get()->GetContentLayerColor(
                ContentLayerType::kSeparatorColor)),
        gfx::Insets::TLBR(kMenuSeparatorVerticalPadding, 0,
                          kMenuSeparatorVerticalPadding - kTraySeparatorWidth,
                          0)));
  } else {
    view->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::VH(kMenuSeparatorVerticalPadding, 0)));
  }
  view->SchedulePaint();
}

HoverHighlightView* DetailedViewDelegate::CreateScrollListItem(
    ViewClickListener* listener,
    const gfx::VectorIcon& icon,
    const std::u16string& text) {
  HoverHighlightView* item = new HoverHighlightView(listener);
  if (icon.is_empty())
    item->AddLabelRow(text);
  else
    item->AddIconAndLabel(
        gfx::CreateVectorIcon(icon,
                              AshColorProvider::Get()->GetContentLayerColor(
                                  ContentLayerType::kIconColorPrimary)),
        text);
  return item;
}

views::Button* DetailedViewDelegate::CreateBackButton(
    views::Button::PressedCallback callback) {
  return new BackButton(std::move(callback));
}

views::Button* DetailedViewDelegate::CreateInfoButton(
    views::Button::PressedCallback callback,
    int info_accessible_name_id) {
  return new IconButton(std::move(callback), IconButton::Type::kSmall,
                        &kUnifiedMenuInfoIcon, info_accessible_name_id);
}

views::Button* DetailedViewDelegate::CreateSettingsButton(
    views::Button::PressedCallback callback,
    int setting_accessible_name_id) {
  auto* button = new IconButton(std::move(callback), IconButton::Type::kSmall,
                                &vector_icons::kSettingsOutlineIcon,
                                setting_accessible_name_id);
  if (!TrayPopupUtils::CanOpenWebUISettings())
    button->SetEnabled(false);
  return button;
}

views::Button* DetailedViewDelegate::CreateHelpButton(
    views::Button::PressedCallback callback) {
  auto* button =
      new IconButton(std::move(callback), IconButton::Type::kSmall,
                     &vector_icons::kHelpOutlineIcon, IDS_ASH_STATUS_TRAY_HELP);
  // Help opens a web page, so treat it like Web UI settings.
  if (!TrayPopupUtils::CanOpenWebUISettings())
    button->SetEnabled(false);
  return button;
}

void DetailedViewDelegate::UpdateColors() {
  if (title_label_) {
    title_label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
  }
  if (title_separator_) {
    title_separator_->SetColorId(ui::kColorAshSystemUIMenuSeparator);
  }
}

}  // namespace ash
