// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_view.h"

#include "ash/ash_element_identifiers.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/style/pagination_view.h"
#include "ash/system/media/quick_settings_media_view_container.h"
#include "ash/system/tray/interacted_by_tap_recorder.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/feature_tiles_container_view.h"
#include "ash/system/unified/quick_settings_footer.h"
#include "ash/system/unified/quick_settings_header.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

constexpr auto kPaginationViewMargin = gfx::Insets::TLBR(0, 0, 8, 0);
constexpr auto kSlidersContainerMargin = gfx::Insets::TLBR(4, 0, 0, 0);

class AccessibilityFocusHelperView : public views::View {
  METADATA_HEADER(AccessibilityFocusHelperView, views::View)

 public:
  explicit AccessibilityFocusHelperView(UnifiedSystemTrayController* controller)
      : controller_(controller) {
    GetViewAccessibility().SetRole(ax::mojom::Role::kListItem);
  }

  bool HandleAccessibleAction(const ui::AXActionData& action_data) override {
    GetFocusManager()->ClearFocus();
    GetFocusManager()->SetStoredFocusView(nullptr);
    return true;
  }

 private:
  raw_ptr<UnifiedSystemTrayController> controller_;
};

BEGIN_METADATA(AccessibilityFocusHelperView)
END_METADATA

}  // namespace

// The container view for the system tray, i.e. the panel containing settings
// buttons and sliders (e.g. sign out, lock, volume slider, etc.).
class QuickSettingsView::SystemTrayContainer : public views::View {
  METADATA_HEADER(SystemTrayContainer, views::View)

 public:
  SystemTrayContainer()
      : layout_manager_(SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical))) {}
  SystemTrayContainer(const SystemTrayContainer&) = delete;
  SystemTrayContainer& operator=(const SystemTrayContainer&) = delete;

  ~SystemTrayContainer() override = default;

  void SetFlexForView(views::View* view) {
    DCHECK_EQ(view->parent(), this);
    layout_manager_->SetFlexForView(view, 1);
  }

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

 private:
  const raw_ptr<views::BoxLayout> layout_manager_;
};

BEGIN_METADATA(QuickSettingsView, SystemTrayContainer)
END_METADATA

QuickSettingsView::QuickSettingsView(UnifiedSystemTrayController* controller)
    : controller_(controller),
      interacted_by_tap_recorder_(
          std::make_unique<InteractedByTapRecorder>(this)) {
  DCHECK(controller_);
  controller_->model()->pagination_model()->AddObserver(this);

  SetProperty(views::kElementIdentifierKey, kQuickSettingsViewElementId);

  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto* scroll_view = AddChildView(std::make_unique<views::ScrollView>());
  scroll_view->SetAllowKeyboardScrolling(false);
  scroll_view->SetBackgroundColor(std::nullopt);
  scroll_view->ClipHeightTo(0, INT_MAX);
  scroll_view->SetDrawOverflowIndicator(false);
  scroll_view->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  system_tray_container_ =
      scroll_view->SetContents(std::make_unique<views::FlexLayoutView>());
  system_tray_container_->SetOrientation(views::LayoutOrientation::kVertical);

  header_ = system_tray_container_->AddChildView(
      std::make_unique<QuickSettingsHeader>(controller_));
  feature_tiles_container_ = system_tray_container_->AddChildView(
      std::make_unique<FeatureTilesContainerView>(controller_));

  // Creates a container for `PaginationView`. This is needed to align the view
  // in the center.
  auto* pagination_view_container = system_tray_container_->AddChildView(
      std::make_unique<views::BoxLayoutView>());
  pagination_view_container->SetOrientation(
      views::BoxLayout::Orientation::kHorizontal);
  pagination_view_container->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  pagination_view_container->SetProperty(views::kMarginsKey,
                                         kPaginationViewMargin);
  pagination_view_ =
      pagination_view_container->AddChildView(std::make_unique<PaginationView>(
          controller_->model()->pagination_model()));

  media_view_container_ = system_tray_container_->AddChildView(
      std::make_unique<QuickSettingsMediaViewContainer>(controller_));

  sliders_container_ = system_tray_container_->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  sliders_container_->SetOrientation(views::LayoutOrientation::kVertical);
  sliders_container_->SetProperty(views::kMarginsKey, kSlidersContainerMargin);

  footer_ = system_tray_container_->AddChildView(
      std::make_unique<QuickSettingsFooter>(controller_));

  detailed_view_container_ = AddChildView(std::make_unique<views::View>());
  detailed_view_container_->SetUseDefaultFillLayout(true);
  detailed_view_container_->SetVisible(false);

  auto* focus_helper_view_ = system_tray_container_->AddChildView(
      std::make_unique<AccessibilityFocusHelperView>(controller_));
  focus_helper_view_->SetID(VIEW_ID_QS_ACCESSIBILITY_FOCUS_HELPER_VIEW);
}

QuickSettingsView::~QuickSettingsView() {
  controller_->model()->pagination_model()->RemoveObserver(this);
}

void QuickSettingsView::SetMaxHeight(int max_height) {
  max_height_ = max_height;
  feature_tiles_container_->SetRowsFromHeight(
      CalculateHeightForFeatureTilesContainer());
}

void QuickSettingsView::AddTiles(
    std::vector<std::unique_ptr<FeatureTile>> tiles) {
  feature_tiles_container_->AddTiles(std::move(tiles));
}

views::View* QuickSettingsView::AddSliderView(
    std::unique_ptr<views::View> slider_view) {
  return sliders_container_->AddChildView(std::move(slider_view));
}

void QuickSettingsView::AddMediaView(std::unique_ptr<views::View> media_view) {
  DCHECK(media_view);
  DCHECK(media_view_container_);
  media_view_container_->AddChildView(std::move(media_view));
}

void QuickSettingsView::SetShowMediaView(bool show_media_view) {
  DCHECK(media_view_container_);
  media_view_container_->SetShowMediaView(show_media_view);
  feature_tiles_container_->AdjustRowsForMediaViewVisibility(
      show_media_view, CalculateHeightForFeatureTilesContainer());
  PreferredSizeChanged();
}

void QuickSettingsView::SetDetailedView(
    std::unique_ptr<views::View> detailed_view) {
  detailed_view_container_->RemoveAllChildViews();
  detailed_view_container_->AddChildView(std::move(detailed_view));
  system_tray_container_->SetVisible(false);
  detailed_view_container_->SetVisible(true);

  // We need to trigger layout here to make sure that `CalendarView` is notified
  // that it can be initialized through `OnViewBoundsChanged`. The
  // `CalendarView` depends on `OnViewsBoundsChanged` to check if it can
  // `ScrollToToday`.
  DeprecatedLayoutImmediately();
}

void QuickSettingsView::ResetDetailedView() {
  detailed_view_container_->RemoveAllChildViews();
  detailed_view_container_->SetVisible(false);
  if (media_view_container_) {
    media_view_container_->MaybeShowMediaView();
  }
  system_tray_container_->SetVisible(true);
}

void QuickSettingsView::SaveFocus() {
  auto* focus_manager = GetFocusManager();
  if (!focus_manager) {
    return;
  }

  saved_focused_view_ = focus_manager->GetFocusedView();
}

void QuickSettingsView::RestoreFocus() {
  if (saved_focused_view_) {
    saved_focused_view_->RequestFocus();
  }
}

int QuickSettingsView::GetCurrentHeight() const {
  return GetPreferredSize().height();
}

// TODO(b/253303697): The `FeatureTilesContainer` does not currently respect
// size constraints when vertical space is limited. This leads to the
// `QuickSettingsView` being clipped from the bottom.
int QuickSettingsView::CalculateHeightForFeatureTilesContainer() {
  int media_view_container_height =
      media_view_container_ ? media_view_container_->GetExpandedHeight() : 0;

  return max_height_ - header_->GetPreferredSize().height() -
         pagination_view_->GetPreferredSize().height() -
         sliders_container_->GetPreferredSize().height() -
         media_view_container_height - footer_->GetPreferredSize().height();
}

std::u16string QuickSettingsView::GetDetailedViewAccessibleName() const {
  return controller_->detailed_view_controller()->GetAccessibleName();
}

bool QuickSettingsView::IsDetailedViewShown() const {
  return detailed_view_container_->GetVisible();
}

void QuickSettingsView::TotalPagesChanged(int previous_page_count,
                                          int new_page_count) {
  pagination_view_->SetVisible(new_page_count > 1);
}

views::View* QuickSettingsView::GetAccessibilityFocusHelperViewForTesting() {
  return system_tray_container_->GetViewByID(
      VIEW_ID_QS_ACCESSIBILITY_FOCUS_HELPER_VIEW);
}

BEGIN_METADATA(QuickSettingsView)
END_METADATA

}  // namespace ash
