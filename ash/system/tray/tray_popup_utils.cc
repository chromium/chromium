// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_popup_utils.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/size_range_layout.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/unfocusable_label.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/slider.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"

namespace ash {

namespace {

// Creates a layout manager that positions Views vertically. The Views will be
// stretched horizontally and centered vertically.
std::unique_ptr<views::LayoutManager> CreateDefaultCenterLayoutManager() {
  // TODO(bruthig): Use constants instead of magic numbers.
  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(8, kTrayPopupLabelHorizontalPadding));
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  return std::move(box_layout);
}

// Creates a layout manager that positions Views horizontally. The Views will be
// centered along the horizontal and vertical axis.
std::unique_ptr<views::LayoutManager> CreateDefaultEndsLayoutManager() {
  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  return std::move(box_layout);
}

std::unique_ptr<views::LayoutManager> CreateDefaultLayoutManager(
    TriView::Container container) {
  switch (container) {
    case TriView::Container::START:
    case TriView::Container::END:
      return CreateDefaultEndsLayoutManager();
    case TriView::Container::CENTER:
      return CreateDefaultCenterLayoutManager();
  }
  // Required by some compilers.
  NOTREACHED();
  return nullptr;
}

// Configures the default size and flex value for the specified |container|
// of the given |tri_view|. Used by CreateDefaultRowView().
void ConfigureDefaultSizeAndFlex(TriView* tri_view,
                                 TriView::Container container) {
  int min_width = 0;
  switch (container) {
    case TriView::Container::START:
      min_width = kTrayPopupItemMinStartWidth;
      break;
    case TriView::Container::CENTER:
      tri_view->SetFlexForContainer(TriView::Container::CENTER, 1.f);
      break;
    case TriView::Container::END:
      min_width = kTrayPopupItemMinEndWidth;
      break;
  }

  tri_view->SetMinSize(container,
                       gfx::Size(min_width, kTrayPopupItemMinHeight));
  constexpr int kTrayPopupItemMaxHeight = 144;
  tri_view->SetMaxSize(container, gfx::Size(SizeRangeLayout::kAbsoluteMaxSize,
                                            kTrayPopupItemMaxHeight));
}

gfx::Insets GetInkDropInsets(TrayPopupInkDropStyle ink_drop_style) {
  if (ink_drop_style == TrayPopupInkDropStyle::HOST_CENTERED ||
      ink_drop_style == TrayPopupInkDropStyle::INSET_BOUNDS) {
    return gfx::Insets(kTrayPopupInkDropInset);
  }
  return gfx::Insets();
}

class HighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  explicit HighlightPathGenerator(TrayPopupInkDropStyle ink_drop_style)
      : ink_drop_style_(ink_drop_style) {}

  HighlightPathGenerator(const HighlightPathGenerator&) = delete;
  HighlightPathGenerator& operator=(const HighlightPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  base::Optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    gfx::RectF bounds = rect;
    bounds.Inset(GetInkDropInsets(ink_drop_style_));
    float corner_radius = 0.f;
    switch (ink_drop_style_) {
      case TrayPopupInkDropStyle::HOST_CENTERED:
        corner_radius = std::min(bounds.width(), bounds.height()) / 2.f;
        bounds.ClampToCenteredSize(gfx::SizeF(corner_radius, corner_radius));
        break;
      case TrayPopupInkDropStyle::INSET_BOUNDS:
        corner_radius = kTrayPopupInkDropCornerRadius;
        break;
      case TrayPopupInkDropStyle::FILL_BOUNDS:
        break;
    }

    return gfx::RRectF(bounds, corner_radius);
  }

 private:
  const TrayPopupInkDropStyle ink_drop_style_;
};

}  // namespace

TriView* TrayPopupUtils::CreateDefaultRowView() {
  TriView* tri_view = CreateMultiTargetRowView();

  tri_view->SetContainerLayout(
      TriView::Container::START,
      CreateDefaultLayoutManager(TriView::Container::START));
  tri_view->SetContainerLayout(
      TriView::Container::CENTER,
      CreateDefaultLayoutManager(TriView::Container::CENTER));
  tri_view->SetContainerLayout(
      TriView::Container::END,
      CreateDefaultLayoutManager(TriView::Container::END));

  return tri_view;
}

TriView* TrayPopupUtils::CreateSubHeaderRowView(bool start_visible) {
  TriView* tri_view = CreateDefaultRowView();
  if (!start_visible) {
    tri_view->SetInsets(gfx::Insets(
        0, kTrayPopupPaddingHorizontal - kTrayPopupLabelHorizontalPadding, 0,
        0));
    tri_view->SetContainerVisible(TriView::Container::START, false);
  }
  return tri_view;
}

TriView* TrayPopupUtils::CreateMultiTargetRowView() {
  TriView* tri_view = new TriView(0 /* padding_between_items */);

  tri_view->SetInsets(gfx::Insets(0, kMenuExtraMarginFromLeftEdge, 0, 0));

  ConfigureDefaultSizeAndFlex(tri_view, TriView::Container::START);
  ConfigureDefaultSizeAndFlex(tri_view, TriView::Container::CENTER);
  ConfigureDefaultSizeAndFlex(tri_view, TriView::Container::END);

  tri_view->SetContainerLayout(TriView::Container::START,
                               std::make_unique<views::FillLayout>());
  tri_view->SetContainerLayout(TriView::Container::CENTER,
                               std::make_unique<views::FillLayout>());
  tri_view->SetContainerLayout(TriView::Container::END,
                               std::make_unique<views::FillLayout>());

  return tri_view;
}

views::Label* TrayPopupUtils::CreateDefaultLabel() {
  views::Label* label = new views::Label();
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetSubpixelRenderingEnabled(false);
  return label;
}

UnfocusableLabel* TrayPopupUtils::CreateUnfocusableLabel() {
  UnfocusableLabel* label = new UnfocusableLabel();
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetSubpixelRenderingEnabled(false);
  return label;
}

views::ImageView* TrayPopupUtils::CreateMainImageView() {
  auto* image = new views::ImageView;
  image->SetPreferredSize(
      gfx::Size(kTrayPopupItemMinStartWidth, kTrayPopupItemMinHeight));
  return image;
}

views::ToggleButton* TrayPopupUtils::CreateToggleButton(
    views::ButtonListener* listener,
    int accessible_name_id) {
  constexpr SkColor kTrackAlpha = 0x66;
  auto GetColor = [](bool is_on, SkAlpha alpha = SK_AlphaOPAQUE) {
    AshColorProvider::ContentLayerType type =
        is_on ? AshColorProvider::ContentLayerType::kIconColorProminent
              : AshColorProvider::ContentLayerType::kTextColorPrimary;

    return SkColorSetA(AshColorProvider::Get()->GetContentLayerColor(type),
                       alpha);
  };
  views::ToggleButton* toggle = new views::ToggleButton(listener);
  const gfx::Size toggle_size(toggle->GetPreferredSize());
  const int vertical_padding = (kMenuButtonSize - toggle_size.height()) / 2;
  const int horizontal_padding =
      (kTrayToggleButtonWidth - toggle_size.width()) / 2;
  toggle->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(vertical_padding, horizontal_padding)));
  toggle->SetAccessibleName(l10n_util::GetStringUTF16(accessible_name_id));
  toggle->SetThumbOnColor(GetColor(true));
  toggle->SetThumbOffColor(GetColor(false));
  toggle->SetTrackOnColor(GetColor(true, kTrackAlpha));
  toggle->SetTrackOffColor(GetColor(false, kTrackAlpha));
  return toggle;
}

std::unique_ptr<views::Painter> TrayPopupUtils::CreateFocusPainter() {
  return views::Painter::CreateSolidFocusPainter(
      UnifiedSystemTrayView::GetFocusRingColor(), kFocusBorderThickness,
      gfx::InsetsF());
}

void TrayPopupUtils::ConfigureTrayPopupButton(views::Button* button) {
  button->SetInstallFocusRingOnFocus(true);
  button->SetFocusForPlatform();
  button->SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
  button->SetHasInkDropActionOnClick(true);
}

void TrayPopupUtils::ConfigureAsStickyHeader(views::View* view) {
  view->SetID(VIEW_ID_STICKY_HEADER);
  view->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kMenuSeparatorVerticalPadding, 0)));
  view->SetPaintToLayer();
  view->layer()->SetFillsBoundsOpaquely(false);
}

void TrayPopupUtils::ConfigureContainer(TriView::Container container,
                                        views::View* container_view) {
  container_view->SetLayoutManager(CreateDefaultLayoutManager(container));
}

views::LabelButton* TrayPopupUtils::CreateTrayPopupButton(
    views::ButtonListener* listener,
    const base::string16& text) {
  auto button = std::make_unique<views::MdTextButton>(listener, text);
  button->SetProminent(true);
  return button.release();
}

views::Separator* TrayPopupUtils::CreateVerticalSeparator() {
  views::Separator* separator = new views::Separator();
  separator->SetPreferredHeight(24);
  separator->SetColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparatorColor));
  return separator;
}

std::unique_ptr<views::InkDrop> TrayPopupUtils::CreateInkDrop(
    views::InkDropHostView* host) {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      std::make_unique<views::InkDropImpl>(host, host->size());
  ink_drop->SetAutoHighlightMode(
      views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
  ink_drop->SetShowHighlightOnHover(false);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropRipple> TrayPopupUtils::CreateInkDropRipple(
    TrayPopupInkDropStyle ink_drop_style,
    const views::View* host,
    const gfx::Point& center_point) {
  const AshColorProvider::RippleAttributes ripple_attributes =
      AshColorProvider::Get()->GetRippleAttributes();
  return std::make_unique<views::FloodFillInkDropRipple>(
      host->size(), GetInkDropInsets(ink_drop_style), center_point,
      ripple_attributes.base_color, ripple_attributes.inkdrop_opacity);
}

std::unique_ptr<views::InkDropHighlight> TrayPopupUtils::CreateInkDropHighlight(
    const views::View* host) {
  const AshColorProvider::RippleAttributes ripple_attributes =
      AshColorProvider::Get()->GetRippleAttributes();
  auto highlight = std::make_unique<views::InkDropHighlight>(
      gfx::SizeF(host->size()), ripple_attributes.base_color);
  highlight->set_visible_opacity(ripple_attributes.highlight_opacity);
  return highlight;
}

void TrayPopupUtils::InstallHighlightPathGenerator(
    views::View* host,
    TrayPopupInkDropStyle ink_drop_style) {
  views::HighlightPathGenerator::Install(
      host, std::make_unique<HighlightPathGenerator>(ink_drop_style));
}

views::Separator* TrayPopupUtils::CreateListItemSeparator(bool left_inset) {
  views::Separator* separator = new views::Separator();
  separator->SetColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparatorColor));
  separator->SetBorder(views::CreateEmptyBorder(
      kMenuSeparatorVerticalPadding - views::Separator::kThickness,
      left_inset ? kMenuExtraMarginFromLeftEdge + kMenuButtonSize +
                       kTrayPopupLabelHorizontalPadding
                 : 0,
      kMenuSeparatorVerticalPadding, 0));
  return separator;
}

bool TrayPopupUtils::CanOpenWebUISettings() {
  return Shell::Get()->session_controller()->ShouldEnableSettings();
}

void TrayPopupUtils::InitializeAsCheckableRow(HoverHighlightView* container,
                                              bool checked,
                                              bool enterprise_managed) {
  const int dip_size = GetDefaultSizeOfVectorIcon(kCheckCircleIcon);
  gfx::ImageSkia check_mark = CreateVectorIcon(
      kHollowCheckCircleIcon, dip_size,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorProminent));
  if (enterprise_managed) {
    gfx::ImageSkia enterprise_managed_icon = CreateVectorIcon(
        kLoginScreenEnterpriseIcon, dip_size, gfx::kGoogleGrey100);
    container->AddRightIcon(enterprise_managed_icon,
                            enterprise_managed_icon.width());
  }
  container->AddRightIcon(check_mark, check_mark.width());
  UpdateCheckMarkVisibility(container, checked);
}

void TrayPopupUtils::UpdateCheckMarkVisibility(HoverHighlightView* container,
                                               bool visible) {
  container->SetRightViewVisible(visible);
  container->SetAccessibilityState(
      visible ? HoverHighlightView::AccessibilityState::CHECKED_CHECKBOX
              : HoverHighlightView::AccessibilityState::UNCHECKED_CHECKBOX);
}

void TrayPopupUtils::SetupTraySubLabel(views::Label* label) {
  label->SetBorder(views::CreateEmptyBorder(kTraySubLabelPadding));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  TrayPopupItemStyle sub_style(TrayPopupItemStyle::FontStyle::CAPTION);
  sub_style.set_color_style(TrayPopupItemStyle::ColorStyle::INACTIVE);
  sub_style.SetupLabel(label);
}

}  // namespace ash
