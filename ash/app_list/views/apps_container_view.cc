// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/apps_container_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/views/app_list_a11y_announcer.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_keyboard_controller.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_nudge_controller.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/app_list/views/app_list_toast_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/continue_section_view.h"
#include "ash/app_list/views/folder_background_view.h"
#include "ash/app_list/views/page_switcher.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_page_dialog_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/shelf_config.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// The number of rows for portrait mode.
constexpr int kPreferredGridRowsInPortrait = 5;

// The number of columns for portrait mode.
constexpr int kPreferredGridColumnsInPortrait = 5;

// The number of columns for landscape mode.
constexpr int kPreferredGridColumns = 5;

// The number of rows for landscape mode.
constexpr int kPreferredGridRows = 4;

// The amount by which the apps container UI should be offset downwards when
// shown on non apps page UI.
constexpr int kNonAppsStateVerticalOffset = 24;

// The opacity the apps container UI should have when shown on non apps page UI.
constexpr float kNonAppsStateOpacity = 0.1;

// The ratio of allowed bounds for apps grid view to its maximum margin.
constexpr int kAppsGridMarginRatio = 16;
constexpr int kAppsGridMarginRatioForSmallHeight = 24;

// The margins within the apps container for app list folder view.
constexpr int kFolderMargin = 16;

// The horizontal margin between the apps grid view and the page switcher.
constexpr int kGridToPageSwitcherMargin = 8;

// Minimal horizontal distance from the page switcher to apps container bounds.
constexpr int kPageSwitcherEndMargin = 16;

// The minimum amount of vertical margin between the apps container edges and
// the its contents.
constexpr int kMinimumVerticalContainerMargin = 24;

// The vertical margin above the `AppsGridView`. The space between the
// search box and the app grid.
constexpr int kAppGridTopMargin = 24;

// The number of columns available for the ContinueSectionView.
constexpr int kContinueColumnCount = 4;

// The vertical spacing between recent apps and continue section view.
constexpr int kRecentAppsTopMargin = 16;

// The vertical spacing above and below the separator when using kRegular/kDense
// AppListConfigType.
constexpr int kRegularSeparatorVerticalInset = 16;
constexpr int kDenseSeparatorVerticalInset = 8;

// The width of the separator.
constexpr int kSeparatorWidth = 240;

// The actual height of the fadeout gradient mask at the top and bottom of the
// `scrollable_container_`.
constexpr int kDefaultFadeoutMaskHeight = 16;

// Max amount of time to wait for zero state results when refreshing recent apps
// and continue section when launcher becomes visible.
constexpr base::TimeDelta kZeroStateSearchTimeout = base::Milliseconds(16);

const ui::DropTargetEvent GetTranslatedDropTargetEvent(
    const ui::DropTargetEvent event,
    views::View* src_view,
    views::View* dst_view) {
  gfx::Point event_location = event.location();
  views::View::ConvertPointToTarget(src_view, dst_view, &event_location);
  return ui::DropTargetEvent(event.data(), gfx::PointF(event_location),
                             event.root_location_f(),
                             event.source_operations());
}

}  // namespace

// A view that contains continue section, recent apps and a separator view,
// which is shown when any of other views is shown.
// The view is intended to be a wrapper around suggested content views that
// makes applying identical transforms to suggested content views easier.
class AppsContainerView::ContinueContainer : public views::View {
  METADATA_HEADER(ContinueContainer, views::View)

 public:
  ContinueContainer(AppListKeyboardController* keyboard_controller,
                    AppListViewDelegate* view_delegate,
                    views::Separator* separator)
      : view_delegate_(view_delegate), separator_(separator) {
    DCHECK(view_delegate_);
    DCHECK(separator_);
    SetPaintToLayer(ui::LAYER_NOT_DRAWN);

    SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kVertical);

    continue_section_ = AddChildView(std::make_unique<ContinueSectionView>(
        view_delegate, kContinueColumnCount,
        /*tablet_mode=*/true));
    continue_section_->SetPaintToLayer();
    continue_section_->layer()->SetFillsBoundsOpaquely(false);

    recent_apps_ = AddChildView(
        std::make_unique<RecentAppsView>(keyboard_controller, view_delegate));
    recent_apps_->SetPaintToLayer();
    recent_apps_->layer()->SetFillsBoundsOpaquely(false);

    UpdateRecentAppsMargins();
    UpdateContinueSectionVisibility();
  }

  // views::View:
  void ChildVisibilityChanged(views::View* child) override {
    if (child == recent_apps_ || child == continue_section_)
      UpdateSeparatorVisibility();

    if (child == continue_section_)
      UpdateRecentAppsMargins();
  }

  bool HasRecentApps() const { return recent_apps_->GetVisible(); }

  void UpdateAppListConfig(AppListConfig* config) {
    recent_apps_->UpdateAppListConfig(config);
  }

  void UpdateContinueSectionVisibility() {
    // The continue section view and recent apps view manage their own
    // visibility internally.
    continue_section_->UpdateElementsVisibility();
    recent_apps_->UpdateVisibility();
    UpdateSeparatorVisibility();
  }

  // Animates a fade-in for the continue section, recent apps and separator.
  void FadeInViews() {
    continue_section_->layer()->SetOpacity(0.0f);
    recent_apps_->layer()->SetOpacity(0.0f);
    separator_->layer()->SetOpacity(0.0f);

    views::AnimationBuilder()
        .SetPreemptionStrategy(ui::LayerAnimator::PreemptionStrategy::
                                   IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .Once()
        .At(base::Milliseconds(100))
        .SetOpacity(continue_section_, 1.0f)
        .SetOpacity(recent_apps_, 1.0f)
        .SetOpacity(separator_, 1.0f)
        .SetDuration(base::Milliseconds(200));
  }

  ContinueSectionView* continue_section() { return continue_section_; }
  RecentAppsView* recent_apps() { return recent_apps_; }

 private:
  void UpdateRecentAppsMargins() {
    // Remove recent apps top margin if continue section is hidden.
    recent_apps_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(
            continue_section_->GetVisible() ? kRecentAppsTopMargin : 0, 0, 0,
            0));
  }

  void UpdateSeparatorVisibility() {
    separator_->SetVisible(recent_apps_->GetVisible() ||
                           continue_section_->GetVisible());
  }

  const raw_ptr<AppListViewDelegate> view_delegate_;
  raw_ptr<ContinueSectionView> continue_section_ = nullptr;
  raw_ptr<RecentAppsView> recent_apps_ = nullptr;
  raw_ptr<views::Separator, DanglingUntriaged> separator_ = nullptr;
};

BEGIN_METADATA(AppsContainerView, ContinueContainer)
END_METADATA

const int AppsContainerView::kHorizontalMargin = 24;

AppsContainerView::AppsContainerView(ContentsView* contents_view)
    : contents_view_(contents_view),
      app_list_keyboard_controller_(
          std::make_unique<AppListKeyboardController>(this)),
      app_list_nudge_controller_(std::make_unique<AppListNudgeController>()) {
  AppListModelProvider::Get()->AddObserver(this);

  SetPaintToLayer(ui::LAYER_NOT_DRAWN);

  scrollable_container_ = AddChildView(std::make_unique<views::View>());
  scrollable_container_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  AppListViewDelegate* view_delegate =
      contents_view_->GetAppListMainView()->view_delegate();

  // The bounds of the |scrollable_container_| will visually clip the
  // |continue_container_| and |apps_grid_view_| layers.
  scrollable_container_->layer()->SetMasksToBounds(true);

  AppListA11yAnnouncer* a11y_announcer =
      contents_view->app_list_view()->a11y_announcer();
  separator_ =
      scrollable_container_->AddChildView(std::make_unique<views::Separator>());
  separator_->SetColorId(ui::kColorAshSystemUIMenuSeparator);
  separator_->SetPreferredSize(
      gfx::Size(kSeparatorWidth, views::Separator::kThickness));
  // Initially set the vertical inset to kRegularSeparatorVerticalInset. The
  // value will be updated in `AppsContainerView::UpdateAppListConfig()`
  separator_->SetProperty(views::kMarginsKey,
                          gfx::Insets::VH(kRegularSeparatorVerticalInset, 0));
  separator_->SetPaintToLayer();
  separator_->layer()->SetFillsBoundsOpaquely(false);
  // Visibility for `separator_` will be managed by the `continue_container_`.
  separator_->SetVisible(false);

  dialog_controller_ = std::make_unique<SearchResultPageDialogController>(
      contents_view_->GetSearchBoxView());

  continue_container_ =
      scrollable_container_->AddChildView(std::make_unique<ContinueContainer>(
          app_list_keyboard_controller_.get(), view_delegate, separator_));
  continue_container_->continue_section()->SetNudgeController(
      app_list_nudge_controller_.get());
  // Update the suggestion tasks after the app list nudge controller is set in
  // continue section.
  continue_container_->continue_section()->UpdateSuggestionTasks();

  // Add a empty container view. A toast view should be added to
  // `toast_container_` when the app list starts temporary sorting.
  toast_container_ = scrollable_container_->AddChildView(
      std::make_unique<AppListToastContainerView>(
          app_list_nudge_controller_.get(), app_list_keyboard_controller_.get(),
          a11y_announcer, view_delegate,
          /*delegate=*/this, /*tablet_mode=*/true));
  toast_container_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);

  apps_grid_view_ =
      scrollable_container_->AddChildView(std::make_unique<PagedAppsGridView>(
          contents_view, a11y_announcer,
          /*folder_controller=*/this,
          /*container_delegate=*/this, app_list_keyboard_controller_.get()));
  apps_grid_view_->pagination_model()->AddObserver(this);
  apps_grid_view_->set_margin_for_gradient_mask(kDefaultFadeoutMaskHeight);

  // Page switcher should be initialized after AppsGridView.
  auto page_switcher =
      std::make_unique<PageSwitcher>(apps_grid_view_->pagination_model());
  page_switcher_ = AddChildView(std::move(page_switcher));

  auto app_list_folder_view =
      std::make_unique<AppListFolderView>(this, apps_grid_view_, a11y_announcer,
                                          view_delegate, /*tablet_mode=*/true);
  folder_background_view_ = AddChildView(
      std::make_unique<FolderBackgroundView>(app_list_folder_view.get()));

  app_list_folder_view_ = AddChildView(std::move(app_list_folder_view));
  // The folder view is initially hidden.
  app_list_folder_view_->SetVisible(false);

  // NOTE: At this point, the apps grid folder and recent apps grids are not
  // fully initialized - they require an `app_list_config_` instance (because
  // they contain AppListItemView), which in turn requires widget, and the
  // view's contents bounds to be correctly calculated. The initialization
  // will be completed in `OnBoundsChanged()` when the apps container bounds are
  // first set.
}

AppsContainerView::~AppsContainerView() {
  AppListModelProvider::Get()->RemoveObserver(this);
  apps_grid_view_->pagination_model()->RemoveObserver(this);

  // Make sure |page_switcher_| is deleted before |apps_grid_view_| because
  // |page_switcher_| uses the PaginationModel owned by |apps_grid_view_|.
  delete page_switcher_;

  // App list folder view, if shown, may reference/observe a root apps grid view
  // item (associated with the item for which the folder is shown). Delete
  // `app_list_folder_view_` explicitly to ensure it's deleted before
  // `apps_grid_view_`.
  delete app_list_folder_view_;
}

void AppsContainerView::UpdateTopLevelGridDimensions() {
  const GridLayout grid_layout = CalculateGridLayout();
  apps_grid_view_->SetMaxColumnsAndRows(
      /*max_columns=*/grid_layout.columns,
      /*max_rows_on_first_page=*/grid_layout.first_page_rows,
      /*max_rows=*/grid_layout.rows);
}

gfx::Rect AppsContainerView::CalculateAvailableBoundsForAppsGrid(
    const gfx::Rect& contents_bounds) const {
  gfx::Rect available_bounds = contents_bounds;
  // Reserve horizontal margins to accommodate page switcher.
  available_bounds.Inset(
      gfx::Insets::VH(0, GetMinHorizontalMarginForAppsGrid()));
  // Reserve vertical space for search box and suggestion chips.
  available_bounds.Inset(gfx::Insets().set_top(GetMinTopMarginForAppsGrid(
      contents_view_->GetSearchBoxSize(AppListState::kStateApps))));
  // Remove space for vertical margins at the top and bottom of the apps
  // container.
  available_bounds.Inset(gfx::Insets::VH(GetIdealVerticalMargin(), 0));

  return available_bounds;
}

void AppsContainerView::UpdateAppListConfig(const gfx::Rect& contents_bounds) {
  // The rows for this grid layout will be ignored during creation of a new
  // config.
  GridLayout grid_layout = CalculateGridLayout();

  const gfx::Rect available_bounds =
      CalculateAvailableBoundsForAppsGrid(contents_bounds);

  std::unique_ptr<AppListConfig> new_config =
      AppListConfigProvider::Get().CreateForTabletAppList(
          display::Screen::GetScreen()
              ->GetDisplayNearestView(GetWidget()->GetNativeView())
              .work_area()
              .size(),
          grid_layout.columns, available_bounds.size(), app_list_config_.get());

  // `CreateForTabletAppList()` will create a new config only if it differs
  // from the current `app_list_config_`. Nothing to do if the old
  // `AppListConfig` can be used for the updated apps container bounds.
  if (!new_config)
    return;

  // Keep old config around until child views have been updated to use the new
  // config.
  auto old_config = std::move(app_list_config_);
  app_list_config_ = std::move(new_config);

  // Invalidate the cached container margins - app list config change generally
  // changes preferred apps grid margins, which can influence the container
  // margins.
  cached_container_margins_ = CachedContainerMargins();

  if (separator_) {
    const int separator_vertical_inset =
        app_list_config_->type() == AppListConfigType::kRegular
            ? kRegularSeparatorVerticalInset
            : kDenseSeparatorVerticalInset;
    separator_->SetProperty(views::kMarginsKey,
                            gfx::Insets::VH(separator_vertical_inset, 0));
  }

  apps_grid_view()->UpdateAppListConfig(app_list_config_.get());
  app_list_folder_view()->UpdateAppListConfig(app_list_config_.get());
  if (continue_container_)
    continue_container_->UpdateAppListConfig(app_list_config_.get());
}

void AppsContainerView::OnActiveAppListModelsChanged(
    AppListModel* model,
    SearchModel* search_model) {
  // Nothing to do if the apps grid views have not yet been initialized.
  if (!app_list_config_)
    return;

  UpdateForActiveAppListModel();
}

void AppsContainerView::ShowFolderForItemView(AppListItemView* folder_item_view,
                                              bool focus_name_input,
                                              base::OnceClosure hide_callback) {
  // Prevent new animations from starting if there are currently animations
  // pending. This fixes crbug.com/357099.
  if (app_list_folder_view_->IsAnimationRunning())
    return;

  DCHECK(folder_item_view->is_folder());

  UMA_HISTOGRAM_ENUMERATION("Apps.AppListFolderOpened",
                            kFullscreenAppListFolders, kMaxFolderOpened);

  app_list_folder_view_->ConfigureForFolderItemView(folder_item_view,
                                                    std::move(hide_callback));
  SetShowState(SHOW_ACTIVE_FOLDER, false);

  // If there is no selected view in the root grid when a folder is opened,
  // silently focus the first item in the folder to avoid showing the selection
  // highlight or announcing to A11y, but still ensuring the arrow keys navigate
  // from the first item.
  if (focus_name_input) {
    app_list_folder_view_->FocusNameInput();
  } else {
    const bool silently = !apps_grid_view()->has_selected_view();
    app_list_folder_view_->FocusFirstItem(silently);
  }
  // Disable all the items behind the folder so that they will not be reached
  // during focus traversal.
  DisableFocusForShowingActiveFolder(true);
}

void AppsContainerView::ShowApps(AppListItemView* folder_item_view,
                                 bool select_folder) {
  DVLOG(1) << __FUNCTION__;
  if (app_list_folder_view_->IsAnimationRunning())
    return;

  const bool animate = !!folder_item_view;
  SetShowState(SHOW_APPS, animate);
  DisableFocusForShowingActiveFolder(false);
  if (folder_item_view) {
    // Focus `folder_item_view` but only show the selection highlight if there
    // was already one showing.
    if (select_folder)
      folder_item_view->RequestFocus();
    else
      folder_item_view->SilentlyRequestFocus();
  }
}

void AppsContainerView::ResetForShowApps() {
  DVLOG(1) << __FUNCTION__;
  UpdateRecentApps();
  SetShowState(SHOW_APPS, false);
  apps_grid_view_->MaybeAbortWholeGridAnimation();
  DisableFocusForShowingActiveFolder(false);

  if (needs_layout()) {
    // Layout might be needed if `ResetForShowApps` was called during animation
    // (specifically, during tablet ->(aborted) clamshell -> tablet transition).
    DeprecatedLayoutImmediately();
  }
}

void AppsContainerView::ReparentFolderItemTransit(
    AppListFolderItem* folder_item) {
  if (app_list_folder_view_->IsAnimationRunning())
    return;
  SetShowState(SHOW_ITEM_REPARENT, false);
  DisableFocusForShowingActiveFolder(false);
}

bool AppsContainerView::IsInFolderView() const {
  return show_state_ == SHOW_ACTIVE_FOLDER;
}

void AppsContainerView::ReparentDragEnded() {
  DVLOG(1) << __FUNCTION__;
  // The container will be showing apps if the folder was deleted mid-drag.
  if (show_state_ == SHOW_APPS)
    return;
  DCHECK_EQ(SHOW_ITEM_REPARENT, show_state_);
  show_state_ = AppsContainerView::SHOW_APPS;
}

void AppsContainerView::OnAppListVisibilityWillChange(bool visible) {
  if (!visible) {
    return;
  }

  // Start zero state search to refresh contents of the continue section and
  // recent apps.
  contents_view_->GetAppListMainView()->view_delegate()->StartZeroStateSearch(
      base::BindOnce(&AppsContainerView::OnZeroStateSearchDone,
                     weak_ptr_factory_.GetWeakPtr()),
      kZeroStateSearchTimeout);
}

void AppsContainerView::OnAppListVisibilityChanged(bool shown) {
  if (toast_container_) {
    // Updates the visibility state in toast container.
    AppListToastContainerView::VisibilityState state =
        shown ? (is_active_page_
                     ? AppListToastContainerView::VisibilityState::kShown
                     : AppListToastContainerView::VisibilityState::
                           kShownInBackground)
              : AppListToastContainerView::VisibilityState::kHidden;
    toast_container_->UpdateVisibilityState(state);

    // Check if the reorder nudge view needs update if the app list is showing.
    if (shown)
      toast_container_->MaybeUpdateReorderNudgeView();
  }

  // Layout requests may get ignored by apps container's view hierarchy while
  // app list animation is in progress - relayout the container if it needs
  // layout at this point.
  // TODO(https://crbug.com/1306613): Remove explicit layout once the linked
  // issue gets fixed.
  if (shown && needs_layout())
    DeprecatedLayoutImmediately();
}

// PaginationModelObserver:
void AppsContainerView::SelectedPageChanged(int old_selected,
                                            int new_selected) {
  // |continue_container_| is hidden above the grid when not on the first page.
  gfx::Transform transform;
  gfx::Vector2dF translate;
  translate.set_y(-scrollable_container_->bounds().height() * new_selected);
  transform.Translate(translate);
  continue_container_->layer()->SetTransform(transform);
  separator_->layer()->SetTransform(transform);
  if (toast_container_)
    toast_container_->layer()->SetTransform(transform);

  if (new_selected == apps_grid_view_->pagination_model()->total_pages() - 1) {
    RecordLauncherWorkflowMetrics(
        AppListUserAction::kNavigatedToBottomOfAppList,
        /*is_tablet_mode = */ true, std::nullopt);
  }
}

void AppsContainerView::TransitionChanged() {
  auto* pagination_model = apps_grid_view_->pagination_model();
  const PaginationModel::Transition& transition =
      pagination_model->transition();
  if (!pagination_model->is_valid_page(transition.target_page))
    return;

  // Because |continue_container_| only shows on the first page, only update its
  // transform if its page is involved in the transition. Otherwise, there is
  // no need to transform the |continue_container_| because it will remain
  // hidden throughout the transition.
  if (transition.target_page == 0 || pagination_model->selected_page() == 0) {
    const int page_height = scrollable_container_->bounds().height();
    gfx::Vector2dF translate;

    if (transition.target_page == 0) {
      // Scroll the continue section down from above.
      translate.set_y(-page_height + page_height * transition.progress);
    } else {
      // Scroll the continue section upwards
      translate.set_y(-page_height * transition.progress);
    }
    gfx::Transform transform;
    transform.Translate(translate);
    continue_container_->layer()->SetTransform(transform);
    separator_->layer()->SetTransform(transform);
    if (toast_container_)
      toast_container_->layer()->SetTransform(transform);
  }
}

void AppsContainerView::TransitionStarted() {
  MaybeCreateGradientMask();
}

void AppsContainerView::TransitionEnded() {
  // TODO(crbug.com/1285184): Sometimes gradient mask is not removed because
  // this function does not get called in some cases.

  // Gradient mask is no longer necessary once transition is finished.
  MaybeRemoveGradientMask();
}

void AppsContainerView::ScrollStarted() {
  MaybeCreateGradientMask();
}

void AppsContainerView::ScrollEnded() {
  // Need to reset the mask because transition will not happen in some
  // cases. (See https://crbug.com/1049275)
  MaybeRemoveGradientMask();
}

// PagedAppsGridView::ContainerDelegate:
bool AppsContainerView::IsPointWithinPageFlipBuffer(
    const gfx::Point& point_in_apps_grid) const {
  // The page flip buffer is the work area bounds excluding shelf bounds, which
  // is the same as AppsContainerView's bounds.
  gfx::Point point = point_in_apps_grid;
  ConvertPointToTarget(apps_grid_view_, this, &point);
  return this->GetContentsBounds().Contains(point);
}

bool AppsContainerView::IsPointWithinBottomDragBuffer(
    const gfx::Point& point,
    int page_flip_zone_size) const {
  // The bottom drag buffer is between the bottom of apps grid and top of shelf.
  gfx::Point point_in_parent = point;
  ConvertPointToTarget(apps_grid_view_, this, &point_in_parent);
  gfx::Rect parent_rect = this->GetContentsBounds();
  const int kBottomDragBufferMax = parent_rect.bottom();
  const int kBottomDragBufferMin = scrollable_container_->bounds().bottom() -
                                   apps_grid_view_->GetInsets().bottom() -
                                   page_flip_zone_size;

  return point_in_parent.y() > kBottomDragBufferMin &&
         point_in_parent.y() < kBottomDragBufferMax;
}

void AppsContainerView::MaybeCreateGradientMask() {
  if (!features::IsBackgroundBlurEnabled())
    return;

  if (!scrollable_container_->layer()->HasGradientMask())
    UpdateGradientMaskBounds();
}

void AppsContainerView::MaybeRemoveGradientMask() {
  if (scrollable_container_->layer()->HasGradientMask() &&
      !keep_gradient_mask_for_cardified_state_) {
    scrollable_container_->layer()->SetGradientMask(
        gfx::LinearGradient::GetEmpty());
  }
}

void AppsContainerView::OnCardifiedStateStarted() {
  keep_gradient_mask_for_cardified_state_ = true;
  MaybeCreateGradientMask();
}

void AppsContainerView::OnCardifiedStateEnded() {
  keep_gradient_mask_for_cardified_state_ = false;
  MaybeRemoveGradientMask();
}

void AppsContainerView::OnNudgeRemoved() {
  const int continue_container_height =
      continue_container_->GetPreferredSize().height();
  const int toast_container_height =
      toast_container_ ? toast_container_->GetPreferredSize().height() : 0;

  apps_grid_view_->ConfigureFirstPagePadding(
      continue_container_height + toast_container_height + GetSeparatorHeight(),
      continue_container_->HasRecentApps());
  UpdateTopLevelGridDimensions();

  apps_grid_view_->AnimateOnNudgeRemoved();
}

void AppsContainerView::UpdateForNewSortingOrder(
    const std::optional<AppListSortOrder>& new_order,
    bool animate,
    base::OnceClosure update_position_closure,
    base::OnceClosure animation_done_closure) {
  DCHECK_EQ(animate, !update_position_closure.is_null());
  DCHECK(!animation_done_closure || animate);

  // A11y announcements must happen before animations, otherwise the undo
  // guidance is spoken first because focus moves immediately to the undo button
  // on the toast.
  if (new_order) {
    if (*new_order != AppListSortOrder::kAlphabeticalEphemeralAppFirst)
      toast_container_->AnnounceSortOrder(*new_order);
  } else if (animate) {
    toast_container_->AnnounceUndoSort();
  }

  if (!animate) {
    // Reordering is not required so update the undo toast and return early.
    app_list_nudge_controller_->OnTemporarySortOrderChanged(new_order);
    toast_container_->OnTemporarySortOrderChanged(new_order);
    HandleFocusAfterSort();
    return;
  }

  // If app list sort order change is animated, hide any open folders as part of
  // animation. If the update is not animated, e.g. when committing sort order,
  // keep the folder open to prevent folder closure when apps within the folder
  // are reordered, or whe the folder gets renamed.
  SetShowState(SHOW_APPS, /*show_apps_with_animation=*/false);
  DisableFocusForShowingActiveFolder(false);

  // If `apps_grid_view_` is under page transition animation, finish the
  // animation before starting the reorder animation.
  ash::PaginationModel* pagination_model = apps_grid_view_->pagination_model();
  if (pagination_model->has_transition())
    pagination_model->FinishAnimation();

  // Abort the old reorder animation if any before closure update to avoid data
  // races on closures.
  apps_grid_view_->MaybeAbortWholeGridAnimation();
  DCHECK(!update_position_closure_);
  update_position_closure_ = std::move(update_position_closure);
  DCHECK(!reorder_animation_done_closure_);
  reorder_animation_done_closure_ = std::move(animation_done_closure);

  views::AnimationBuilder animation_builder =
      apps_grid_view_->FadeOutVisibleItemsForReorder(base::BindRepeating(
          &AppsContainerView::OnAppsGridViewFadeOutAnimationEnded,
          weak_ptr_factory_.GetWeakPtr(), new_order));

  // Configure the toast fade out animation if the toast is going to be hidden.
  const bool current_toast_visible = toast_container_->IsToastVisible();
  const bool target_toast_visible =
      toast_container_->GetVisibilityForSortOrder(new_order);
  if (current_toast_visible && !target_toast_visible) {
    animation_builder.GetCurrentSequence().SetOpacity(toast_container_->layer(),
                                                      0.f);
  }
}

void AppsContainerView::UpdateContinueSectionVisibility() {
  if (!continue_container_)
    return;

  // Get the continue container's height before DeprecatedLayoutImmediately().
  const int initial_height = continue_container_->height();

  // Update continue container visibility and bounds.
  continue_container_->UpdateContinueSectionVisibility();
  DeprecatedLayoutImmediately();

  // Only play animations if the tablet mode app list is visible. This function
  // can be called in clamshell mode when the tablet app list is cached.
  if (contents_view_->app_list_view()->app_list_state() ==
      AppListViewState::kClosed) {
    return;
  }

  // The change in continue container height is the amount by which the apps
  // grid view will be offset.
  const int vertical_offset = initial_height - continue_container_->height();

  AppListViewDelegate* view_delegate =
      contents_view_->GetAppListMainView()->view_delegate();
  if (view_delegate->ShouldHideContinueSection()) {
    // Continue section is being hidden. Slide each row of app icons up with a
    // different offset per row.
    apps_grid_view_->SlideVisibleItemsForHideContinueSection(vertical_offset);

    // Don't try to fade out the views on hide because they are already
    // invisible.
    return;
  }

  // Continue section is being shown. Transform the apps grid view up to its
  // original pre-layout position.
  gfx::Transform transform;
  transform.Translate(0, vertical_offset);
  apps_grid_view_->SetTransform(transform);

  // Animate to the identity transform to slide the apps grid view down to its
  // final position.
  views::AnimationBuilder()
      .SetPreemptionStrategy(ui::LayerAnimator::PreemptionStrategy::
                                 IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetTransform(apps_grid_view_, gfx::Transform(),
                    gfx::Tween::ACCEL_LIN_DECEL_100_3)
      .SetDuration(base::Milliseconds(300));

  // Fade in the continue tasks and recent apps views.
  continue_container_->FadeInViews();
}

ContinueSectionView* AppsContainerView::GetContinueSectionView() {
  if (!continue_container_)
    return nullptr;
  return continue_container_->continue_section();
}

RecentAppsView* AppsContainerView::GetRecentAppsView() {
  if (!continue_container_)
    return nullptr;
  return continue_container_->recent_apps();
}

AppsGridView* AppsContainerView::GetAppsGridView() {
  return apps_grid_view_;
}

AppListToastContainerView* AppsContainerView::GetToastContainerView() {
  return toast_container_;
}

void AppsContainerView::UpdateControlVisibility(
    AppListViewState app_list_state) {
  if (app_list_state == AppListViewState::kClosed)
    return;

  SetCanProcessEventsWithinSubtree(app_list_state ==
                                   AppListViewState::kFullscreenAllApps);

  apps_grid_view_->UpdateControlVisibility(app_list_state);
  page_switcher_->SetVisible(
      app_list_state == AppListViewState::kFullscreenAllApps ||
      app_list_state == AppListViewState::kFullscreenSearch);
}

void AppsContainerView::Layout(PassKey) {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  LayoutSuperclass<views::View>(this);

  const int app_list_y =
      GetAppListY(contents_view_->app_list_view()->app_list_state());

  // Set bounding box for the folder view - the folder may overlap with
  // suggestion chips, but not the search box.
  gfx::Rect folder_bounding_box = rect;
  int top_folder_inset = app_list_y;
  int bottom_folder_inset = kFolderMargin;

  top_folder_inset += kFolderMargin;

  // Account for the hotseat which overlaps with contents bounds in tablet mode.
  bottom_folder_inset += ShelfConfig::Get()->hotseat_bottom_padding();

  folder_bounding_box.Inset(gfx::Insets::TLBR(
      top_folder_inset, kFolderMargin, bottom_folder_inset, kFolderMargin));
  app_list_folder_view_->SetBoundingBox(folder_bounding_box);

  // Leave the same available bounds for the apps grid view in both
  // fullscreen and peeking state to avoid resizing the view during
  // animation and dragging, which is an expensive operation.
  rect.set_y(app_list_y);
  rect.set_height(rect.height() -
                  GetAppListY(AppListViewState::kFullscreenAllApps));

  // Layout apps grid.
  const gfx::Insets grid_insets = apps_grid_view_->GetInsets();
  const gfx::Insets margins = CalculateMarginsForAvailableBounds(
      GetContentsBounds(),
      contents_view_->GetSearchBoxSize(AppListState::kStateApps));
  gfx::Rect grid_rect = rect;
  grid_rect.Inset(gfx::Insets::TLBR(kAppGridTopMargin, margins.left(),
                                    margins.bottom(), margins.right()));
  // The grid rect insets are added to calculated margins. Given that the
  // grid bounds rect should include insets, they have to be removed from
  // added margins.
  grid_rect.Inset(-grid_insets);

  gfx::Rect scrollable_bounds = grid_rect;
  // Add space to the top of the `scrollable_container_` bounds to make room for
  // the gradient mask to be placed above the continue section.
  scrollable_bounds.Inset(
      gfx::Insets::TLBR(-kDefaultFadeoutMaskHeight, 0, 0, 0));
  scrollable_container_->SetBoundsRect(scrollable_bounds);

  if (scrollable_container_->layer()->HasGradientMask())
    UpdateGradientMaskBounds();

  bool separator_need_centering = false;
  bool first_page_config_changed = false;

  const int continue_container_height =
      continue_container_->GetPreferredSize().height();
  continue_container_->SetBoundsRect(gfx::Rect(0, kDefaultFadeoutMaskHeight,
                                               grid_rect.width(),
                                               continue_container_height));
  const int toast_container_height =
      toast_container_ ? toast_container_->GetPreferredSize().height() : 0;
  if (toast_container_) {
    toast_container_->SetBoundsRect(gfx::Rect(
        0, continue_container_->bounds().bottom() + GetSeparatorHeight(),
        grid_rect.width(), toast_container_height));
  }

  // When no views are shown between the recent apps and the apps grid,
  // vertically center the separator between them.
  if (toast_container_height == 0 && continue_container_->HasRecentApps())
    separator_need_centering = true;

  // Setting this offset prevents the app items in the grid from overlapping
  // with the continue section.
  first_page_config_changed = apps_grid_view_->ConfigureFirstPagePadding(
      continue_container_height + toast_container_height + GetSeparatorHeight(),
      continue_container_->HasRecentApps());

  // Make sure that UpdateTopLevelGridDimensions() happens after setting the
  // apps grid's first page offset, because it can change the number of rows
  // shown in the grid.
  UpdateTopLevelGridDimensions();

  gfx::Rect apps_grid_bounds(grid_rect.size());
  // Set the apps grid bounds y to make room for the top gradient mask.
  apps_grid_bounds.set_y(kDefaultFadeoutMaskHeight);

  if (apps_grid_view_->bounds() != apps_grid_bounds) {
    apps_grid_view_->SetBoundsRect(apps_grid_bounds);
  } else if (first_page_config_changed) {
    // Apps grid layout depends on the continue container bounds, so explicitly
    // call layout to ensure apps grid view gets laid out even if its bounds do
    // not change.
    apps_grid_view_->DeprecatedLayoutImmediately();
  }

  if (separator_) {
    if (separator_need_centering) {
      // Center the separator between the recent apps and the first row of the
      // apps grid. This is done after the apps grid layout so the correct
      // tile padding is used.
      const int centering_offset =
          continue_container_->bounds().bottom() +
          apps_grid_view_->GetUnscaledFirstPageTilePadding() +
          GetSeparatorHeight() / 2;
      separator_->SetBoundsRect(
          gfx::Rect(gfx::Point((grid_rect.width() - kSeparatorWidth) / 2,
                               centering_offset),
                    gfx::Size(kSeparatorWidth, 1)));
    } else {
      separator_->SetBoundsRect(gfx::Rect(
          (grid_rect.width() - kSeparatorWidth) / 2,
          continue_container_->bounds().bottom() +
              separator_->GetProperty(views::kMarginsKey)->height() / 2,
          kSeparatorWidth, 1));
    }
  }

  // Record the distance of y position between suggestion chip container
  // and apps grid view to avoid duplicate calculation of apps grid view's
  // y position during dragging.
  scrollable_container_y_distance_ = scrollable_container_->y() - app_list_y;

  // Layout page switcher.
  const int page_switcher_width = page_switcher_->GetPreferredSize().width();
  const gfx::Rect page_switcher_bounds(
      grid_rect.right() + kGridToPageSwitcherMargin, scrollable_container_->y(),
      page_switcher_width, grid_rect.height());
  page_switcher_->SetBoundsRect(page_switcher_bounds);

  switch (show_state_) {
    case SHOW_APPS:
      break;
    case SHOW_ACTIVE_FOLDER: {
      app_list_folder_view_->UpdatePreferredBounds();
      folder_background_view_->SetBoundsRect(rect);
      app_list_folder_view_->SetBoundsRect(
          app_list_folder_view_->preferred_bounds());
      break;
    }
    case SHOW_ITEM_REPARENT:
    case SHOW_NONE:
      break;
  }
}

bool AppsContainerView::OnKeyPressed(const ui::KeyEvent& event) {
  if (show_state_ == SHOW_APPS)
    return apps_grid_view_->OnKeyPressed(event);
  else
    return app_list_folder_view_->OnKeyPressed(event);
}

void AppsContainerView::OnBoundsChanged(const gfx::Rect& old_bounds) {
  const bool creating_initial_config = !app_list_config_;

  // The size and layout of apps grid items depend on the dimensions of the
  // display on which the apps container is shown. Given that the apps container
  // is shown in fullscreen app list view (and covers complete app list view
  // bounds), changes in the `AppsContainerView` bounds can be used as a proxy
  // to detect display size changes.
  UpdateAppListConfig(GetContentsBounds());
  DCHECK(app_list_config_);
  UpdateTopLevelGridDimensions();

  // Finish initialization of views that require app list config.
  if (creating_initial_config)
    UpdateForActiveAppListModel();
}

void AppsContainerView::AddedToWidget() {
  GetFocusManager()->AddFocusChangeListener(this);
}

void AppsContainerView::RemovedFromWidget() {
  GetFocusManager()->RemoveFocusChangeListener(this);
}

void AppsContainerView::OnDidChangeFocus(View* focused_before,
                                         View* focused_now) {
  // Ensure that `continue_container_` is visible (the first page is active)
  // after moving focus down from the last row on 2nd+ page to the search box
  // and then to `continue_container_`.
  if (!is_active_page_)
    return;
  if (!continue_container_ || !continue_container_->Contains(focused_now))
    return;
  if (apps_grid_view_->pagination_model()->selected_page() != 0)
    apps_grid_view_->pagination_model()->SelectPage(0, /*animate=*/false);
}

void AppsContainerView::OnGestureEvent(ui::GestureEvent* event) {
  // Ignore tap/long-press, allow those to pass to the ancestor view.
  if (event->type() == ui::EventType::kGestureTap ||
      event->type() == ui::EventType::kGestureLongPress) {
    return;
  }

  // Will forward events to |apps_grid_view_| if they occur in the same y-region
  if (event->type() == ui::EventType::kGestureScrollBegin &&
      event->location().y() <= apps_grid_view_->bounds().y()) {
    return;
  }

  // If a folder is currently opening or closing, we should ignore the event.
  // This is here until the animation for pagination while closing folders is
  // fixed: https://crbug.com/875133
  if (app_list_folder_view_->IsAnimationRunning()) {
    event->SetHandled();
    return;
  }

  // Temporary event for use by |apps_grid_view_|
  ui::GestureEvent grid_event(*event);
  ConvertEventToTarget(apps_grid_view_, &grid_event);
  apps_grid_view_->OnGestureEvent(&grid_event);

  // If the temporary event was handled, we don't want to handle it again.
  if (grid_event.handled())
    event->SetHandled();
}

void AppsContainerView::OnShown() {
  DVLOG(1) << __FUNCTION__;
  // Explicitly hide the virtual keyboard before showing the apps container
  // view. This prevents the virtual keyboard's "transient blur" feature from
  // kicking in - if a text input loses focus, and a text input gains it within
  // seconds, the virtual keyboard gets reshown. This is undesirable behavior
  // for the app list (where search box gets focus by default).
  if (keyboard::KeyboardUIController::HasInstance())
    keyboard::KeyboardUIController::Get()->HideKeyboardExplicitlyBySystem();

  GetViewAccessibility().SetIsLeaf(false);
  is_active_page_ = true;

  // Update the continue section.
  if (continue_container_)
    continue_container_->continue_section()->SetShownInBackground(false);

  // Updates the visibility state in toast container.
  if (toast_container_) {
    toast_container_->UpdateVisibilityState(
        AppListToastContainerView::VisibilityState::kShown);
  }
  if (dialog_controller_)
    dialog_controller_->Reset(/*enabled=*/true);
}

void AppsContainerView::OnWillBeHidden() {
  DVLOG(1) << __FUNCTION__;
  if (show_state_ == SHOW_ACTIVE_FOLDER)
    app_list_folder_view_->CloseFolderPage();
  else
    apps_grid_view_->CancelDragWithNoDropAnimation();
}

void AppsContainerView::OnHidden() {
  // Apps container view is shown faded behind the search results UI - hide its
  // contents from the screen reader as the apps grid is not normally
  // actionable in this state.
  GetViewAccessibility().SetIsLeaf(true);

  is_active_page_ = false;

  // Update the continue section.
  if (continue_container_)
    continue_container_->continue_section()->SetShownInBackground(true);

  // Updates the visibility state in toast container.
  if (toast_container_) {
    toast_container_->UpdateVisibilityState(
        AppListToastContainerView::VisibilityState::kShownInBackground);
  }
  if (dialog_controller_)
    dialog_controller_->Reset(/*enabled=*/false);
}

void AppsContainerView::OnAnimationStarted(AppListState from_state,
                                           AppListState to_state) {
  gfx::Rect contents_bounds = GetDefaultContentsBounds();

  const gfx::Rect from_rect =
      GetPageBoundsForState(from_state, contents_bounds, gfx::Rect());
  const gfx::Rect to_rect =
      GetPageBoundsForState(to_state, contents_bounds, gfx::Rect());
  if (from_rect != to_rect) {
    DCHECK_EQ(from_rect.size(), to_rect.size());
    DCHECK_EQ(from_rect.x(), to_rect.x());

    SetBoundsRect(to_rect);

    gfx::Transform initial_transform;
    initial_transform.Translate(0, from_rect.y() - to_rect.y());
    layer()->SetTransform(initial_transform);

    auto settings = contents_view_->CreateTransitionAnimationSettings(layer());
    layer()->SetTransform(gfx::Transform());
  }

  // Set the page opacity.
  auto settings = contents_view_->CreateTransitionAnimationSettings(layer());
  UpdateContainerOpacityForState(to_state);
}

void AppsContainerView::UpdatePageOpacityForState(AppListState state,
                                                  float search_box_opacity) {
  UpdateContainerOpacityForState(state);
}

void AppsContainerView::UpdatePageBoundsForState(
    AppListState state,
    const gfx::Rect& contents_bounds,
    const gfx::Rect& search_box_bounds) {
  AppListPage::UpdatePageBoundsForState(state, contents_bounds,
                                        search_box_bounds);

  UpdateContentsYPosition(contents_view_->app_list_view()->app_list_state());
}

gfx::Rect AppsContainerView::GetPageBoundsForState(
    AppListState state,
    const gfx::Rect& contents_bounds,
    const gfx::Rect& search_box_bounds) const {
  if (state == AppListState::kStateApps)
    return contents_bounds;

  gfx::Rect bounds = contents_bounds;
  bounds.Offset(0, kNonAppsStateVerticalOffset);
  return bounds;
}

int AppsContainerView::GetMinHorizontalMarginForAppsGrid() const {
  return kPageSwitcherEndMargin + kGridToPageSwitcherMargin +
         page_switcher_->GetPreferredSize().width();
}

int AppsContainerView::GetMinTopMarginForAppsGrid(
    const gfx::Size& search_box_size) const {
  return search_box_size.height() + kAppGridTopMargin;
}

int AppsContainerView::GetIdealVerticalMargin() const {
  const int screen_height =
      display::Screen::GetScreen()
          ->GetDisplayNearestView(GetWidget()->GetNativeView())
          .bounds()
          .height();
  const float margin_ratio = (screen_height <= 800)
                                 ? kAppsGridMarginRatioForSmallHeight
                                 : kAppsGridMarginRatio;

  return std::max(kMinimumVerticalContainerMargin,
                  static_cast<int>(screen_height / margin_ratio));
}

const gfx::Insets& AppsContainerView::CalculateMarginsForAvailableBounds(
    const gfx::Rect& available_bounds,
    const gfx::Size& search_box_size) {
  if (cached_container_margins_.bounds_size == available_bounds.size() &&
      cached_container_margins_.search_box_size == search_box_size) {
    return cached_container_margins_.margins;
  }

  // `app_list_config_` is required for apps_grid_view to calculate the tile
  // grid sizes.
  DCHECK(app_list_config_);

  // The `grid_layout`'s rows will be ignored because the vertical margin will
  // be constant.
  const GridLayout grid_layout = CalculateGridLayout();
  const gfx::Size min_grid_size = apps_grid_view()->GetMinimumTileGridSize(
      grid_layout.columns, grid_layout.rows);
  const gfx::Size max_grid_size = apps_grid_view()->GetMaximumTileGridSize(
      grid_layout.columns, grid_layout.rows);

  // Calculates margin value to ensure the apps grid size is within required
  // bounds.
  // |ideal_margin|: The value the margin would have with no restrictions on
  //                 grid size.
  // |available_size|: The available size for apps grid in the dimension where
  //                   margin is applied.
  // |min_size|: The min allowed size for apps grid in the dimension where
  //             margin is applied.
  // |max_size|: The max allowed size for apps grid in the dimension where
  //             margin is applied.
  const auto calculate_margin = [](int ideal_margin, int available_size,
                                   int min_size, int max_size) -> int {
    const int ideal_size = available_size - 2 * ideal_margin;
    if (ideal_size < min_size)
      return ideal_margin - (min_size - ideal_size + 1) / 2;
    if (ideal_size > max_size)
      return ideal_margin + (ideal_size - max_size) / 2;
    return ideal_margin;
  };

  // The grid will change the number of rows to fit within the provided space.
  int vertical_margin = GetIdealVerticalMargin();

  const int horizontal_margin =
      calculate_margin(kHorizontalMargin, available_bounds.width(),
                       min_grid_size.width(), max_grid_size.width());

  const int min_horizontal_margin = GetMinHorizontalMarginForAppsGrid();

  cached_container_margins_.margins = gfx::Insets::TLBR(
      std::max(vertical_margin, kMinimumVerticalContainerMargin),
      std::max(horizontal_margin, min_horizontal_margin),
      std::max(vertical_margin, kMinimumVerticalContainerMargin),
      std::max(horizontal_margin, min_horizontal_margin));
  cached_container_margins_.bounds_size = available_bounds.size();
  cached_container_margins_.search_box_size = search_box_size;

  return cached_container_margins_.margins;
}

void AppsContainerView::UpdateRecentApps() {
  RecentAppsView* recent_apps = GetRecentAppsView();
  if (!recent_apps || !app_list_config_)
    return;

  AppListModelProvider* const model_provider = AppListModelProvider::Get();
  recent_apps->SetModels(model_provider->search_model(),
                         model_provider->model());
}

void AppsContainerView::SetShowState(ShowState show_state,
                                     bool show_apps_with_animation) {
  if (show_state_ == show_state)
    return;

  show_state_ = show_state;

  // Layout before showing animation because the animation's target bounds are
  // calculated based on the layout.
  DeprecatedLayoutImmediately();

  switch (show_state_) {
    case SHOW_APPS:
      page_switcher_->SetCanProcessEventsWithinSubtree(true);
      folder_background_view_->SetVisible(false);
      apps_grid_view_->ResetForShowApps();
      app_list_folder_view_->ResetItemsGridForClose();
      if (show_apps_with_animation) {
        app_list_folder_view_->ScheduleShowHideAnimation(false, false);
      } else {
        app_list_folder_view_->HideViewImmediately();
      }
      break;
    case SHOW_ACTIVE_FOLDER:
      page_switcher_->SetCanProcessEventsWithinSubtree(false);
      folder_background_view_->SetVisible(true);
      app_list_folder_view_->ScheduleShowHideAnimation(true, false);
      break;
    case SHOW_ITEM_REPARENT:
      page_switcher_->SetCanProcessEventsWithinSubtree(true);
      folder_background_view_->SetVisible(false);
      app_list_folder_view_->ScheduleShowHideAnimation(false, true);
      break;
    default:
      NOTREACHED();
  }
}

void AppsContainerView::UpdateContainerOpacityForState(AppListState state) {
  const float target_opacity =
      state == AppListState::kStateApps ? 1.0f : kNonAppsStateOpacity;
  if (layer()->GetTargetOpacity() != target_opacity)
    layer()->SetOpacity(target_opacity);
}

void AppsContainerView::UpdateContentsYPosition(AppListViewState state) {
  const int app_list_y = GetAppListY(state);
  scrollable_container_->SetY(app_list_y + scrollable_container_y_distance_);
  page_switcher_->SetY(app_list_y + scrollable_container_y_distance_);
}

void AppsContainerView::DisableFocusForShowingActiveFolder(bool disabled) {
  if (auto* recent_apps = GetRecentAppsView(); recent_apps) {
    recent_apps->DisableFocusForShowingActiveFolder(disabled);
  }
  if (auto* continue_section = GetContinueSectionView(); continue_section) {
    continue_section->DisableFocusForShowingActiveFolder(disabled);
  }
  if (toast_container_) {
    toast_container_->DisableFocusForShowingActiveFolder(disabled);
  }
  apps_grid_view_->DisableFocusForShowingActiveFolder(disabled);

  // Ignore the page switcher in accessibility tree so that buttons inside it
  // will not be accessed by ChromeVox.
  SetViewIgnoredForAccessibility(page_switcher_, disabled);
}

int AppsContainerView::GetAppListY(AppListViewState state) {
  const gfx::Rect search_box_bounds =
      contents_view_->GetSearchBoxBounds(AppListState::kStateApps);
  return search_box_bounds.bottom();
}

AppsContainerView::GridLayout AppsContainerView::CalculateGridLayout() const {
  DCHECK(GetWidget());

  // Adapt columns and rows based on the display/root window size.
  const gfx::Size size =
      display::Screen::GetScreen()
          ->GetDisplayNearestView(GetWidget()->GetNativeView())
          .work_area()
          .size();
  const bool is_portrait_mode = size.height() > size.width();
  const int available_height =
      CalculateAvailableBoundsForAppsGrid(GetContentsBounds()).height();

  int preferred_columns = 0;
  int preferred_rows = 0;
  int preferred_rows_first_page = 0;

  if (is_portrait_mode) {
    preferred_rows = kPreferredGridRowsInPortrait;
    preferred_rows_first_page = preferred_rows;
    preferred_columns = kPreferredGridColumnsInPortrait;
  } else {
    preferred_rows = kPreferredGridRows;
    preferred_rows_first_page = preferred_rows;

    // In landscape mode, the first page should show the preferred number of
    // rows as well as an additional row for recent apps when possible.
    if (continue_container_ && continue_container_->HasRecentApps())
      preferred_rows_first_page++;

    preferred_columns = kPreferredGridColumns;
  }

  GridLayout result;
  result.columns = preferred_columns;
  result.rows =
      apps_grid_view_->CalculateMaxRows(available_height, preferred_rows);
  result.first_page_rows = apps_grid_view_->CalculateFirstPageMaxRows(
      available_height, preferred_rows_first_page);
  return result;
}

void AppsContainerView::UpdateForActiveAppListModel() {
  AppListModel* const model = AppListModelProvider::Get()->model();
  apps_grid_view_->SetModel(model);
  apps_grid_view_->SetItemList(model->top_level_item_list());
  UpdateRecentApps();

  // If model changes, close the folder view if it's open, as the associated
  // item list is about to go away.
  SetShowState(SHOW_APPS, false);
}

void AppsContainerView::UpdateGradientMaskBounds() {
  if (scrollable_container_->bounds().IsEmpty())
    return;

  // Vertical linear gradient from top to bottom.
  gfx::LinearGradient gradient_mask(/*angle=*/-90);
  float fade_in_out_fraction = static_cast<float>(kDefaultFadeoutMaskHeight) /
                               scrollable_container_->bounds().height();
  // Fade in section.
  gradient_mask.AddStep(/*fraction=*/0, /*alpha=*/0);
  gradient_mask.AddStep(fade_in_out_fraction, 255);
  // Fade out section
  gradient_mask.AddStep((1 - fade_in_out_fraction), 255);
  gradient_mask.AddStep(1, 0);

  if (gradient_mask != scrollable_container_->layer()->gradient_mask())
    scrollable_container_->layer()->SetGradientMask(gradient_mask);
}

void AppsContainerView::OnAppsGridViewFadeOutAnimationEnded(
    const std::optional<AppListSortOrder>& new_order,
    bool abort) {
  // Update item positions after the fade out animation but before the fade in
  // animation. NOTE: `update_position_closure_` can be empty in some edge
  // cases. For example, the app list is set with a new order denoted by Order
  // A. Then before the fade out animation is completed, the app list order is
  // reset with the old value. In this case, `update_position_closure_` for
  // Order A is never called. As a result, the closure for resetting the order
  // is empty.
  // Also update item positions only when the fade out animation ends normally.
  // Because a fade out animation is aborted when:
  // (1) Another reorder animation starts, or
  // (2) The apps grid's view model updates due to the reasons such as app
  // installation or model reset.
  // It is meaningless to update item positions in either case.
  if (update_position_closure_ && !abort)
    std::move(update_position_closure_).Run();

  // Record the undo toast's visibility before update.
  const bool old_toast_visible = toast_container_->IsToastVisible();

  toast_container_->OnTemporarySortOrderChanged(new_order);
  HandleFocusAfterSort();

  // Skip the fade in animation if the fade out animation is aborted.
  if (abort) {
    OnReorderAnimationEnded();
    return;
  }

  const bool target_toast_visible = toast_container_->IsToastVisible();
  const bool toast_visibility_change =
      (old_toast_visible != target_toast_visible);

  // When the undo toast's visibility changes, the apps grid's bounds should
  // change. Meanwhile, the fade in animation relies on the apps grid's bounds
  // (because of calculating the visible items). Therefore trigger layout before
  // starting the fade in animation.
  if (toast_visibility_change)
    DeprecatedLayoutImmediately();

  ash::PaginationModel* pagination_model = apps_grid_view_->pagination_model();
  bool page_change = (pagination_model->selected_page() != 0);
  if (page_change) {
    // Ensure that the undo toast is within the view port after reorder.
    pagination_model->SelectPage(0, /*animate=*/false);
  }

  views::AnimationBuilder animation_builder =
      apps_grid_view_->FadeInVisibleItemsForReorder(base::BindRepeating(
          &AppsContainerView::OnAppsGridViewFadeInAnimationEnded,
          weak_ptr_factory_.GetWeakPtr()));

  // Fade in the undo toast when:
  // (1) The toast's visibility becomes true from false, or
  // (2) The apps page is scrolled to show the toast.
  const bool should_fade_in_toast =
      (target_toast_visible && (page_change || toast_visibility_change));

  if (!should_fade_in_toast)
    return;

  // Hide the toast to prepare for the fade in animation,
  toast_container_->layer()->SetOpacity(0.f);

  animation_builder.GetCurrentSequence().SetOpacity(
      toast_container_->layer(), 1.f, gfx::Tween::ACCEL_5_70_DECEL_90);

  // Continue section should be faded in only when the page changes.
  if (page_change) {
    continue_container_->layer()->SetOpacity(0.f);
    animation_builder.GetCurrentSequence().SetOpacity(
        continue_container_->layer(), 1.f, gfx::Tween::ACCEL_5_70_DECEL_90);
  }
}

void AppsContainerView::OnAppsGridViewFadeInAnimationEnded(bool aborted) {
  if (aborted) {
    // Ensure that children are visible when the fade in animation is aborted.
    toast_container_->layer()->SetOpacity(1.f);
    continue_container_->layer()->SetOpacity(1.f);
  }

  OnReorderAnimationEnded();
}

void AppsContainerView::OnReorderAnimationEnded() {
  update_position_closure_.Reset();

  if (reorder_animation_done_closure_)
    std::move(reorder_animation_done_closure_).Run();
}

void AppsContainerView::HandleFocusAfterSort() {
  // As the sort update on AppsContainerView can be called in both clamshell
  // mode and tablet mode, return early if it's currently in clamshell mode
  // because the AppsContainerView isn't visible.
  if (contents_view_->app_list_view()->app_list_state() ==
      AppListViewState::kClosed) {
    return;
  }

  // If the sort is done and the toast is visible and not fading out, request
  // the focus on the undo button on the toast. Otherwise request the focus on
  // the search box.
  if (toast_container_->IsToastVisible()) {
    toast_container_->toast_view()->toast_button()->RequestFocus();
  } else {
    contents_view_->GetSearchBoxView()->search_box()->RequestFocus();
  }
}

int AppsContainerView::GetSeparatorHeight() {
  if (!separator_ || !separator_->GetVisible())
    return 0;
  return separator_->GetProperty(views::kMarginsKey)->height() +
         views::Separator::kThickness;
}

void AppsContainerView::OnZeroStateSearchDone() {
  UpdateRecentApps();
  if (needs_layout()) {
    // NOTE: Request another layout after recent apps get updated to handle the
    // case when recent apps get updated during app list state change animation.
    // The apps container layout may get dropped by the app list contents view,
    // so invalidating recent apps layout when recent apps visibiltiy changes
    // will not work well).
    // TODO(b/261662349): Remove explicit layout once the linked issue is fixed.
    DeprecatedLayoutImmediately();
  }
}

bool AppsContainerView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  return apps_grid_view_->GetDropFormats(formats, format_types);
}

bool AppsContainerView::CanDrop(const OSExchangeData& data) {
  return apps_grid_view_->WillAcceptDropEvent(data);
}

void AppsContainerView::OnDragExited() {
  apps_grid_view_->OnDragExited();
}

void AppsContainerView::OnDragEntered(const ui::DropTargetEvent& event) {
  apps_grid_view_->OnDragEntered(
      GetTranslatedDropTargetEvent(event, this, apps_grid_view_));
}

int AppsContainerView::OnDragUpdated(const ui::DropTargetEvent& event) {
  return apps_grid_view_->OnDragUpdated(
      GetTranslatedDropTargetEvent(event, this, apps_grid_view_));
}

views::View::DropCallback AppsContainerView::GetDropCallback(
    const ui::DropTargetEvent& event) {
  return apps_grid_view_->GetDropCallback(
      GetTranslatedDropTargetEvent(event, this, apps_grid_view_));
}

BEGIN_METADATA(AppsContainerView)
END_METADATA

}  // namespace ash
