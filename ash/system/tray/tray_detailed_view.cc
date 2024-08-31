// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_detailed_view.h"

#include <cstring>
#include <string>
#include <utility>

#include "ash/controls/rounded_scroll_bar.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/typography.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_targeter.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr int kScrollViewCornerRadius = 16;

// Inset the scroll bar to avoid the rounded corners at top and bottom.
constexpr auto kScrollBarInsets = gfx::Insets::VH(kScrollViewCornerRadius, 0);

// Configures the TriView used for the title in a detailed view.
void ConfigureTitleTriView(TriView* tri_view, TriView::Container container) {
  std::unique_ptr<views::BoxLayout> layout;

  switch (container) {
    case TriView::Container::START:
    case TriView::Container::END: {
      const int left_padding = container == TriView::Container::START
                                   ? kUnifiedBackButtonLeftPadding
                                   : 0;
      const int right_padding =
          container == TriView::Container::END ? kTitleRightPadding : 0;
      layout = std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::TLBR(0, left_padding, 0, right_padding),
          kTitleItemBetweenSpacing);
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
          views::BoxLayout::CrossAxisAlignment::kCenter);
      break;
  }

  tri_view->SetContainerLayout(container, std::move(layout));
  tri_view->SetMinSize(container,
                       gfx::Size(0, kUnifiedDetailedViewTitleRowHeight));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TrayDetailedView:

TrayDetailedView::TrayDetailedView(DetailedViewDelegate* delegate)
    : delegate_(delegate) {
  box_layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
}

TrayDetailedView::~TrayDetailedView() = default;

void TrayDetailedView::OnViewClicked(views::View* sender) {
  DCHECK(sender);
  HandleViewClicked(sender);
}

void TrayDetailedView::OverrideProgressBarAccessibleName(
    const std::u16string& name) {
  progress_bar_accessible_name_ = name;
}

void TrayDetailedView::CreateTitleRow(int string_id) {
  DCHECK(!tri_view_);

  tri_view_ = AddChildViewAt(CreateTitleTriView(string_id), 0);

  back_button_ = delegate_->CreateBackButton(base::BindRepeating(
      &TrayDetailedView::TransitionToMainView, base::Unretained(this)));
  back_button_->SetID(VIEW_ID_QS_DETAILED_VIEW_BACK_BUTTON);
  tri_view_->AddView(TriView::Container::START, back_button_);

  // Adds an empty view as a placeholder so that the views below won't move up
  // when the `progress_bar_` becomes invisible.
  auto buffer_view = std::make_unique<views::View>();
  buffer_view->SetPreferredSize(gfx::Size(1, kTitleRowProgressBarHeight));
  AddChildViewAt(std::move(buffer_view), kTitleRowProgressBarIndex);
  CreateExtraTitleRowButtons();

  // Makes the `tri_view_`'s `START` and `END`container have the same width,
  // so the header text will be in the center of the `QuickSettingsView`
  // horizontally.
  auto* start_view =
      tri_view_->children()[static_cast<size_t>(TriView::Container::START)]
          .get();
  auto* end_view =
      tri_view_->children()[static_cast<size_t>(TriView::Container::END)].get();
  int start_width = start_view->GetPreferredSize().width();
  int end_width = end_view->GetPreferredSize().width();
  if (start_width < end_width) {
    DCHECK(start_view->GetVisible());
    start_view->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(0, 0, 0, end_width - start_width)));
    start_view->InvalidateLayout();
  } else {
    // Ensure the end container is visible, even if it has no buttons.
    tri_view_->SetContainerVisible(TriView::Container::END, true);
    end_view->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(0, start_width - end_width, 0, 0)));
  }

  DeprecatedLayoutImmediately();
}

void TrayDetailedView::CreateScrollableList() {
  DCHECK(!scroller_);
  scroller_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroller_->SetDrawOverflowIndicator(false);
  scroll_content_ = scroller_->SetContents(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kVertical)
          .Build());

  auto vertical_scroll = std::make_unique<RoundedScrollBar>(
      views::ScrollBar::Orientation::kVertical);
  vertical_scroll->SetInsets(kScrollBarInsets);
  vertical_scroll->SetAlwaysShowThumb(true);
  scroller_->SetVerticalScrollBar(std::move(vertical_scroll));
  scroller_->SetProperty(views::kMarginsKey, delegate_->GetScrollViewMargin());
  scroller_->SetPaintToLayer();
  scroller_->layer()->SetFillsBoundsOpaquely(false);
  scroller_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kScrollViewCornerRadius));

  // Override the default theme-based color to remove the background.
  scroller_->SetBackgroundColor(std::nullopt);

  box_layout_->SetFlexForView(scroller_, 1);
}

HoverHighlightView* TrayDetailedView::AddScrollListItem(
    views::View* container,
    const gfx::VectorIcon& icon,
    const std::u16string& text) {
  HoverHighlightView* item = container->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));
  if (icon.is_empty()) {
    item->AddLabelRow(text);
  } else {
    item->AddIconAndLabel(
        ui::ImageModel::FromVectorIcon(icon, cros_tokens::kCrosSysOnSurface),
        text);
  }
  views::FocusRing::Install(item);
  views::InstallRoundRectHighlightPathGenerator(item, gfx::Insets(2),
                                                /*corner_radius=*/0);
  views::FocusRing::Get(item)->SetColorId(cros_tokens::kCrosSysFocusRing);
  // Unset the focus painter set by `HoverHighlightView`.
  item->SetFocusPainter(nullptr);

  return item;
}

void TrayDetailedView::CreateZeroStateView(
    std::unique_ptr<ZeroStateView> view) {
  CHECK(!zero_state_view_);
  CHECK(scroller());
  zero_state_view_ =
      AddChildViewAt(std::move(view), GetIndexOf(scroller_).value());
  box_layout()->SetFlexForView(zero_state_view_, 1);
  zero_state_view_->SetVisible(false);
}

HoverHighlightView* TrayDetailedView::AddScrollListCheckableItem(
    views::View* container,
    const gfx::VectorIcon& icon,
    const std::u16string& text,
    bool checked,
    bool enterprise_managed) {
  HoverHighlightView* item = AddScrollListItem(container, icon, text);
  if (enterprise_managed) {
    item->GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
        IDS_ASH_ACCESSIBILITY_FEATURE_MANAGED, text));
  }
  TrayPopupUtils::InitializeAsCheckableRow(item, checked, enterprise_managed);
  return item;
}

void TrayDetailedView::Reset() {
  RemoveAllChildViews();
  scroller_ = nullptr;
  scroll_content_ = nullptr;
  progress_bar_ = nullptr;
  back_button_ = nullptr;
  tri_view_ = nullptr;
  zero_state_view_ = nullptr;
}

void TrayDetailedView::ShowProgress(double value, bool visible) {
  DCHECK(tri_view_);
  if (!progress_bar_) {
    progress_bar_ = AddChildViewAt(std::make_unique<views::ProgressBar>(),
                                   kTitleRowProgressBarIndex + 1);
    progress_bar_->SetPreferredHeight(kTitleRowProgressBarHeight);
    progress_bar_->GetViewAccessibility().SetName(
        progress_bar_accessible_name_.value_or(l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_PROGRESS_BAR_ACCESSIBLE_NAME)),
        ax::mojom::NameFrom::kAttribute);
    progress_bar_->SetVisible(false);
    progress_bar_->SetForegroundColor(
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorProminent));
  }

  progress_bar_->SetValue(value);
  progress_bar_->SetVisible(visible);
  children()[size_t{kTitleRowProgressBarIndex}]->SetVisible(!visible);
}

void TrayDetailedView::SetZeroStateViewVisibility(bool visible) {
  CHECK(zero_state_view_);
  zero_state_view_->SetVisible(visible);
  scroller_->SetVisible(!visible);
}

views::Button* TrayDetailedView::CreateInfoButton(
    views::Button::PressedCallback callback,
    int info_accessible_name_id) {
  return delegate_->CreateInfoButton(std::move(callback),
                                     info_accessible_name_id);
}

views::Button* TrayDetailedView::CreateSettingsButton(
    views::Button::PressedCallback callback,
    int setting_accessible_name_id) {
  return delegate_->CreateSettingsButton(std::move(callback),
                                         setting_accessible_name_id);
}

views::Button* TrayDetailedView::CreateHelpButton(
    views::Button::PressedCallback callback) {
  return delegate_->CreateHelpButton(std::move(callback));
}

void TrayDetailedView::HandleViewClicked(views::View* view) {
  NOTREACHED();
}

std::unique_ptr<TriView> TrayDetailedView::CreateTitleTriView(int string_id) {
  auto tri_view = std::make_unique<TriView>(kUnifiedTopShortcutSpacing);

  ConfigureTitleTriView(tri_view.get(), TriView::Container::START);
  ConfigureTitleTriView(tri_view.get(), TriView::Container::CENTER);
  ConfigureTitleTriView(tri_view.get(), TriView::Container::END);

  auto* title_label = TrayPopupUtils::CreateDefaultLabel();
  title_label->SetText(l10n_util::GetStringUTF16(string_id));
  title_label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  ash::TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosTitle1,
                                             *title_label);
  tri_view->AddView(TriView::Container::CENTER, title_label);
  tri_view->SetContainerVisible(TriView::Container::END, false);

  return tri_view;
}

void TrayDetailedView::CreateExtraTitleRowButtons() {}

void TrayDetailedView::TransitionToMainView() {
  delegate_->TransitionToMainView(back_button_ && back_button_->HasFocus());
}

void TrayDetailedView::CloseBubble() {
  // widget may be null in tests, in this case we do not need to do anything.
  views::Widget* widget = GetWidget();
  if (!widget) {
    return;
  }
  // Don't close again if we're already closing.
  if (widget->IsClosed()) {
    return;
  }
  delegate_->CloseBubble();
}

void TrayDetailedView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);
  if (scroller_ && !scroller_->is_bounded()) {
    scroller_->ClipHeightTo(0, scroller_->height());
  }
}

gfx::Size TrayDetailedView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size preferred_size =
      views::View::CalculatePreferredSize(available_size);
  if (bounds().IsEmpty()) {
    return preferred_size;
  }

  // The height of the bubble that contains this detailed view is set to
  // the preferred height of the default view, and that determines the
  // initial height of |this|. Always request to stay the same height.
  return gfx::Size(preferred_size.width(), height());
}

BEGIN_METADATA(TrayDetailedView)
END_METADATA

}  // namespace ash
