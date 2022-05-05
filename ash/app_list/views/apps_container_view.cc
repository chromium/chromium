// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/apps_container_view.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/views/app_list_a11y_announcer.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_nudge_controller.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/app_list/views/app_list_toast_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/continue_section_view.h"
#include "ash/app_list/views/folder_background_view.h"
#include "ash/app_list/views/page_switcher.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_page_dialog_controller.h"
#include "ash/app_list/views/suggestion_chip_container_view.h"
#include "ash/constants/ash_features.h"
#include "ash/controls/gradient_layer_delegate.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_model_delegate.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/search_box/search_box_constants.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/cxx17_backports.h"
#include "base/metrics/histogram_macros.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

// The number of rows for portrait mode with mode productivity launcher
// enabled.
constexpr int kPreferredGridRowsInPortraitProductivityLauncher = 5;

// The number of columns for portrait mode with productivity launcher enabled.
constexpr int kPreferredGridColumnsInPortraitProductivityLauncher = 5;

// The long apps grid dimension when productivity launcher is not enabled:
// * number of columns in landscape mode
// * number of rows in portrait mode
constexpr int kPreferredGridColumns = 5;

// The short apps grid dimension when productivity launcher is not enabled:
// * number of rows in landscape mode
// * number of columns in portrait mode
constexpr int kPreferredGridRows = 4;

// The range of app list transition progress in which the suggestion chips'
// opacity changes from 0 to 1.
constexpr float kSuggestionChipOpacityStartProgress = 0.66;
constexpr float kSuggestionChipOpacityEndProgress = 1;

// Range of the height of centerline above screen bottom that all apps should
// change opacity. NOTE: this is used to change page switcher's opacity as
// well.
constexpr float kAppsOpacityChangeStart = 8.0f;
constexpr float kAppsOpacityChangeEnd = 144.0f;

// The app list transition progress value for fullscreen state.
constexpr float kAppListFullscreenProgressValue = 2.0;

// The amount by which the apps container UI should be offset downwards when
// shown on non apps page UI.
constexpr int kNonAppsStateVerticalOffset = 24;

// The opacity the apps container UI should have when shown on non apps page UI.
constexpr float kNonAppsStateOpacity = 0.1;

// The ratio of allowed bounds for apps grid view to its maximum margin.
constexpr int kAppsGridMarginRatio = 16;
constexpr int kAppsGridMarginRatioForSmallWidth = 12;
constexpr int kAppsGridMarginRatioForSmallHeight = 24;

// The margins within the apps container for app list folder view.
constexpr int kFolderMargin = 16;

// The suggestion chip container height.
constexpr int kSuggestionChipContainerHeight = 32;

// The suggestion chip container top margin.
constexpr int kSuggestionChipContainerTopMargin = 16;

// The horizontal margin between the apps grid view and the page switcher.
constexpr int kGridToPageSwitcherMargin = 8;

// Minimal horizontal distance from the page switcher to apps container bounds.
constexpr int kPageSwitcherEndMargin = 16;

// The minimum amount of vertical margin between the apps container edges and
// the its contents.
constexpr int kMinimumVerticalContainerMargin = 24;

// The vertical margin above the `AppsGridView`. The space between suggestion
// chips and the app grid. With Productivity launcher, the space between the
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

}  // namespace

// A view that contains continue section, recent apps and a separator view,
// which is shown when any of other views is shown.
// The view is intended to be a wrapper around suggested content views that
// makes applying identical transforms to suggested content views easier.
class AppsContainerView::ContinueContainer : public views::View {
 public:
  ContinueContainer(AppsContainerView* apps_container,
                    AppListViewDelegate* view_delegate,
                    SearchResultPageDialogController* dialog_controller)
      : view_delegate_(view_delegate), separator_(apps_container->separator()) {
    DCHECK(view_delegate_);
    DCHECK(separator_);
    SetPaintToLayer(ui::LAYER_NOT_DRAWN);

    SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kVertical);

    // Add the button to show the continue section, wrapped in a view to center
    // it horizontally.
    auto* button_container = AddChildView(std::make_unique<views::View>());
    button_container
        ->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical))
        ->set_cross_axis_alignment(
            views::BoxLayout::CrossAxisAlignment::kCenter);
    show_continue_section_button_ =
        button_container->AddChildView(std::make_unique<PillButton>(
            base::BindRepeating(&AppsContainerView::ContinueContainer::
                                    OnPressShowContinueSection,
                                base::Unretained(this)),
            l10n_util::GetStringUTF16(IDS_ASH_LAUNCHER_SHOW_CONTINUE_SECTION),
            PillButton::Type::kIcon, &kExpandAllIcon));
    show_continue_section_button_->SetUseDefaultLabelFont();
    // Put the icon on the right.
    show_continue_section_button_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);

    continue_section_ = AddChildView(std::make_unique<ContinueSectionView>(
        view_delegate, dialog_controller, kContinueColumnCount,
        /*tablet_mode=*/true));
    continue_section_->SetPaintToLayer();
    continue_section_->layer()->SetFillsBoundsOpaquely(false);

    recent_apps_ = AddChildView(
        std::make_unique<RecentAppsView>(apps_container, view_delegate));
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

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    // The "show continue section" button appears directly over the wallpaper,
    // so use a "base layer" color for its background.
    show_continue_section_button_->SetBackgroundColor(
        AshColorProvider::Get()->GetBaseLayerColor(
            AshColorProvider::BaseLayerType::kTransparent40));
  }

  bool HasRecentApps() const { return recent_apps_->GetVisible(); }

  void UpdateAppListConfig(AppListConfig* config) {
    recent_apps_->UpdateAppListConfig(config);
  }

  void UpdateContinueSectionVisibility() {
    // Show the "Show continue section" button if continue section is hidden.
    bool hide_continue_section = view_delegate_->ShouldHideContinueSection();
    show_continue_section_button_->SetVisible(hide_continue_section);
    // The continue section view and recent apps view manage their own
    // visibility internally.
    continue_section_->UpdateElementsVisibility();
    recent_apps_->UpdateVisibility();
    UpdateSeparatorVisibility();
  }

  PillButton* show_continue_section_button() {
    return show_continue_section_button_;
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

  void OnPressShowContinueSection(const ui::Event& event) {
    view_delegate_->SetHideContinueSection(false);
    UpdateContinueSectionVisibility();
  }

  AppListViewDelegate* const view_delegate_;
  PillButton* show_continue_section_button_ = nullptr;
  ContinueSectionView* continue_section_ = nullptr;
  RecentAppsView* recent_apps_ = nullptr;
  views::Separator* separator_ = nullptr;
};

AppsContainerView::AppsContainerView(ContentsView* contents_view)
    : contents_view_(contents_view),
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
  if (features::IsProductivityLauncherEnabled()) {
    separator_ = scrollable_container_->AddChildView(
        std::make_unique<views::Separator>());
    separator_->SetColor(ColorProvider::Get()->GetContentLayerColor(
        ColorProvider::ContentLayerType::kSeparatorColor));
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
            this, view_delegate, dialog_controller_.get()));
    continue_container_->continue_section()->SetNudgeController(
        app_list_nudge_controller_.get());
    // Update the suggestion tasks after the app list nudge controller is set in
    // continue section.
    continue_container_->continue_section()->UpdateSuggestionTasks();

    // Add a empty container view. A toast view should be added to
    // `toast_container_` when the app list starts temporary sorting.
    if (features::IsLauncherAppSortEnabled()) {
      toast_container_ = scrollable_container_->AddChildView(
          std::make_unique<AppListToastContainerView>(
              app_list_nudge_controller_.get(), a11y_announcer, view_delegate,
              /*delegate=*/this, /*tablet_mode=*/true));
      toast_container_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
    }
  } else {
    // Add child view at index 0 so focus traversal goes to suggestion chips
    // before the views in the scrollable_container.
    suggestion_chip_container_view_ = AddChildViewAt(
        std::make_unique<SuggestionChipContainerView>(contents_view), 0);
  }

  apps_grid_view_ = scrollable_container_->AddChildView(
      std::make_unique<PagedAppsGridView>(contents_view, a11y_announcer,
                                          /*folder_delegate=*/nullptr,
                                          /*folder_controller=*/this,
                                          /*container_delegate=*/this));
  apps_grid_view_->Init();
  apps_grid_view_->pagination_model()->AddObserver(this);
  if (features::IsProductivityLauncherEnabled())
    apps_grid_view_->set_margin_for_gradient_mask(kDefaultFadeoutMaskHeight);

  // Page switcher should be initialized after AppsGridView.
  auto page_switcher = std::make_unique<PageSwitcher>(
      apps_grid_view_->pagination_model(), true /* vertical */,
      contents_view->app_list_view()->is_tablet_mode());
  page_switcher_ = AddChildView(std::move(page_switcher));

  auto app_list_folder_view = std::make_unique<AppListFolderView>(
      this, apps_grid_view_, contents_view_, a11y_announcer, view_delegate);
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
  if (features::IsProductivityLauncherEnabled()) {
    available_bounds.Inset(gfx::Insets::VH(GetIdealVerticalMargin(), 0));
  } else {
    available_bounds.Inset(gfx::Insets::VH(kMinimumVerticalContainerMargin, 0));
  }

  return available_bounds;
}

void AppsContainerView::UpdateAppListConfig(const gfx::Rect& contents_bounds) {
  // For productivity launcher, the rows for this grid layout will be ignored
  // during creation of a new config.
  GridLayout grid_layout = CalculateGridLayout();

  const gfx::Rect available_bounds =
      CalculateAvailableBoundsForAppsGrid(contents_bounds);

  std::unique_ptr<AppListConfig> new_config =
      AppListConfigProvider::Get().CreateForFullscreenAppList(
          display::Screen::GetScreen()
              ->GetDisplayNearestView(GetWidget()->GetNativeView())
              .work_area()
              .size(),
          grid_layout.rows, grid_layout.columns, available_bounds.size(),
          app_list_config_.get());

  // `CreateForFullscreenAppList()` will create a new config only if it differs
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
  UpdateSuggestionChips();
  UpdateRecentApps(/*needs_layout=*/false);
  SetShowState(SHOW_APPS, false);
  DisableFocusForShowingActiveFolder(false);
}

void AppsContainerView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  apps_grid_view()->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
  app_list_folder_view()->items_grid_view()->SetDragAndDropHostOfCurrentAppList(
      drag_and_drop_host);
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
  // Start zero state search to refresh contents of the continue section and
  // recent apps (which are only shown for productivity launcher).
  // NOTE: Request another layout after recent apps get updated to handle the
  // case when recent apps get updated during app list state change animation.
  // The apps container layout may get dropped by the app list  contents view,
  // so invalidating recent apps layout when recent apps visibiltiy changes
  // will not work well).
  // TODO(https://crbug.com/1306613): Remove explicit layout once the linked
  // issue is fixed.
  if (visible && features::IsProductivityLauncherEnabled()) {
    contents_view_->GetAppListMainView()->view_delegate()->StartZeroStateSearch(
        base::BindOnce(&AppsContainerView::UpdateRecentApps,
                       weak_ptr_factory_.GetWeakPtr(),
                       /*needs_layout=*/true),
        kZeroStateSearchTimeout);
  }
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
    Layout();
}

// PaginationModelObserver:
void AppsContainerView::SelectedPageChanged(int old_selected,
                                            int new_selected) {
  // There is no |continue_container_| to translate when productivity launcher
  // is not enabled, so return early.
  if (!features::IsProductivityLauncherEnabled())
    return;

  // |continue_container_| is hidden above the grid when not on the first page.
  gfx::Transform transform;
  gfx::Vector2dF translate;
  translate.set_y(-scrollable_container_->bounds().height() * new_selected);
  transform.Translate(translate);
  continue_container_->layer()->SetTransform(transform);
  separator_->layer()->SetTransform(transform);
  if (toast_container_)
    toast_container_->layer()->SetTransform(transform);
}

void AppsContainerView::TransitionChanged() {
  // There is no |continue_container_| to translate when productivity launcher
  // is not enabled, so return early.
  if (!features::IsProductivityLauncherEnabled())
    return;

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
  if (features::IsBackgroundBlurEnabled()) {
    if (!layer()->layer_mask_layer() && !gradient_layer_delegate_) {
      gradient_layer_delegate_ =
          std::make_unique<GradientLayerDelegate>(/*animate_in=*/false);
      UpdateGradientMaskBounds();
    }
    if (gradient_layer_delegate_) {
      scrollable_container_->layer()->SetMaskLayer(
          gradient_layer_delegate_->layer());
    }
  }
}

void AppsContainerView::MaybeRemoveGradientMask() {
  if (scrollable_container_->layer()->layer_mask_layer() &&
      !keep_gradient_mask_for_cardified_state_) {
    scrollable_container_->layer()->SetMaskLayer(nullptr);
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

// RecentAppsView::Delegate:
void AppsContainerView::MoveFocusUpFromRecents() {
  DCHECK(!GetRecentApps()->children().empty());
  views::View* first_recent = GetRecentApps()->children()[0];
  DCHECK(views::IsViewClass<AppListItemView>(first_recent));
  // Find the view one step in reverse from the first recent app.
  views::View* previous_view = GetFocusManager()->GetNextFocusableView(
      first_recent, GetWidget(), /*reverse=*/true, /*dont_loop=*/false);
  DCHECK(previous_view);
  previous_view->RequestFocus();
}

void AppsContainerView::MoveFocusDownFromRecents(int column) {
  if (toast_container_ && toast_container_->HandleFocus(column))
    return;

  int top_level_item_count = apps_grid_view_->view_model()->view_size();
  if (top_level_item_count <= 0)
    return;
  // Attempt to focus the item at `column` in the first row, or the last item if
  // there aren't enough items. This could happen if the user's apps are in a
  // small number of folders.
  int index = std::min(column, top_level_item_count - 1);
  AppListItemView* item = apps_grid_view_->GetItemViewAt(index);
  DCHECK(item);
  item->RequestFocus();
}

bool AppsContainerView::MoveFocusUpFromToast(int column) {
  return false;
}

bool AppsContainerView::MoveFocusDownFromToast(int column) {
  return false;
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
    const absl::optional<AppListSortOrder>& new_order,
    bool animate,
    base::OnceClosure update_position_closure,
    base::OnceClosure animation_done_closure) {
  DCHECK(features::IsLauncherAppSortEnabled());
  DCHECK_EQ(animate, !update_position_closure.is_null());
  DCHECK(!animation_done_closure || animate);

  // A11y announcements must happen before animations, otherwise the undo
  // guidance is spoken first because focus moves immediately to the undo button
  // on the toast.
  if (new_order) {
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
  apps_grid_view_->MaybeAbortReorderAnimation();
  DCHECK(!update_position_closure_);
  update_position_closure_ = std::move(update_position_closure);
  DCHECK(!reorder_animation_done_closure_);
  reorder_animation_done_closure_ = std::move(animation_done_closure);

  views::AnimationBuilder animation_builder =
      apps_grid_view_->FadeOutVisibleItemsForReorder(base::BindRepeating(
          &AppsContainerView::OnAppsGridViewFadeOutAnimationEnded,
          weak_ptr_factory_.GetWeakPtr(), new_order));

  // Configure the toast fade out animation if the toast is going to be hidden.
  const bool current_toast_visible = toast_container_->is_toast_visible();
  const bool target_toast_visible =
      toast_container_->GetVisibilityForSortOrder(new_order);
  if (current_toast_visible && !target_toast_visible) {
    animation_builder.GetCurrentSequence().SetOpacity(toast_container_->layer(),
                                                      0.f);
  }
}

void AppsContainerView::UpdateContinueSectionVisibility() {
  if (continue_container_)
    continue_container_->UpdateContinueSectionVisibility();
}

ContinueSectionView* AppsContainerView::GetContinueSection() {
  if (!continue_container_)
    return nullptr;
  return continue_container_->continue_section();
}

RecentAppsView* AppsContainerView::GetRecentApps() {
  if (!continue_container_)
    return nullptr;
  return continue_container_->recent_apps();
}

void AppsContainerView::UpdateControlVisibility(AppListViewState app_list_state,
                                                bool is_in_drag) {
  if (app_list_state == AppListViewState::kClosed)
    return;

  SetCanProcessEventsWithinSubtree(
      app_list_state == AppListViewState::kFullscreenAllApps ||
      app_list_state == AppListViewState::kPeeking);

  apps_grid_view_->UpdateControlVisibility(app_list_state, is_in_drag);
  page_switcher_->SetVisible(
      is_in_drag || app_list_state == AppListViewState::kFullscreenAllApps ||
      app_list_state == AppListViewState::kFullscreenSearch);

  // Ignore button press during dragging to avoid app list item views' opacity
  // being set to wrong value.
  page_switcher_->set_ignore_button_press(is_in_drag);

  if (suggestion_chip_container_view_) {
    suggestion_chip_container_view_->SetVisible(
        app_list_state == AppListViewState::kFullscreenAllApps ||
        app_list_state == AppListViewState::kPeeking || is_in_drag);
  }
}

void AppsContainerView::AnimateOpacity(float current_progress,
                                       AppListViewState target_view_state,
                                       const OpacityAnimator& animator) {
  if (suggestion_chip_container_view_) {
    const bool target_suggestion_chip_visibility =
        target_view_state == AppListViewState::kFullscreenAllApps ||
        target_view_state == AppListViewState::kPeeking;
    animator.Run(suggestion_chip_container_view_,
                 target_suggestion_chip_visibility);
  }

  if (!apps_grid_view_->layer()->GetAnimator()->IsAnimatingProperty(
          ui::LayerAnimationElement::OPACITY)) {
    apps_grid_view_->UpdateOpacity(true /*restore_opacity*/,
                                   kAppsOpacityChangeStart,
                                   kAppsOpacityChangeEnd);
    apps_grid_view_->layer()->SetOpacity(current_progress > 1.0f ? 1.0f : 0.0f);
  }

  const bool target_grid_visibility =
      target_view_state == AppListViewState::kFullscreenAllApps ||
      target_view_state == AppListViewState::kFullscreenSearch;
  animator.Run(apps_grid_view_, target_grid_visibility);
  animator.Run(page_switcher_, target_grid_visibility);
}

void AppsContainerView::AnimateYPosition(AppListViewState target_view_state,
                                         const TransformAnimator& animator,
                                         float default_offset) {
  // Apps container position is calculated for app list progress relative to
  // peeking state, which may not match the progress value used to calculate
  // |default_offset| - when showing search results page, the transform offset
  // is calculated using progress relative to AppListViewState::kHalf.
  const float progress =
      contents_view_->app_list_view()->GetAppListTransitionProgress(
          AppListView::kProgressFlagNone |
          AppListView::kProgressFlagWithTransform);
  const int current_suggestion_chip_y = GetExpectedSuggestionChipY(progress);
  const int target_suggestion_chip_y = GetExpectedSuggestionChipY(
      AppListView::GetTransitionProgressForState(target_view_state));
  const int offset = current_suggestion_chip_y - target_suggestion_chip_y;

  if (suggestion_chip_container_view_) {
    suggestion_chip_container_view_->SetY(target_suggestion_chip_y);
    animator.Run(offset, suggestion_chip_container_view_->layer());
  }

  scrollable_container_->SetY(target_suggestion_chip_y + chip_grid_y_distance_);
  animator.Run(offset, scrollable_container_->layer());
  page_switcher_->SetY(target_suggestion_chip_y + chip_grid_y_distance_);
  animator.Run(offset, page_switcher_->layer());
}

void AppsContainerView::OnTabletModeChanged(bool started) {
  if (suggestion_chip_container_view_)
    suggestion_chip_container_view_->OnTabletModeChanged(started);
  apps_grid_view_->OnTabletModeChanged(started);
  app_list_folder_view_->OnTabletModeChanged(started);
  page_switcher_->set_is_tablet_mode(started);
}

void AppsContainerView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  // Layout suggestion chips.
  gfx::Rect chip_container_rect = rect;
  chip_container_rect.set_y(GetExpectedSuggestionChipY(
      contents_view_->app_list_view()->GetAppListTransitionProgress(
          AppListView::kProgressFlagNone)));

  if (suggestion_chip_container_view_) {
    chip_container_rect.set_height(kSuggestionChipContainerHeight);
    chip_container_rect.Inset(gfx::Insets::VH(0, GetIdealHorizontalMargin()));
    suggestion_chip_container_view_->SetBoundsRect(chip_container_rect);
  } else {
    chip_container_rect.set_height(0);
  }

  // Set bounding box for the folder view - the folder may overlap with
  // suggestion chips, but not the search box.
  gfx::Rect folder_bounding_box = rect;
  int top_folder_inset = chip_container_rect.y();
  int bottom_folder_inset = kFolderMargin;

  if (features::IsProductivityLauncherEnabled())
    top_folder_inset += kFolderMargin;

  // Account for the hotseat which overlaps with contents bounds in tablet mode.
  if (contents_view_->app_list_view()->is_tablet_mode())
    bottom_folder_inset += ShelfConfig::Get()->hotseat_bottom_padding();

  folder_bounding_box.Inset(gfx::Insets::TLBR(
      top_folder_inset, kFolderMargin, bottom_folder_inset, kFolderMargin));
  app_list_folder_view_->SetBoundingBox(folder_bounding_box);

  // Leave the same available bounds for the apps grid view in both
  // fullscreen and peeking state to avoid resizing the view during
  // animation and dragging, which is an expensive operation.
  rect.set_y(chip_container_rect.bottom());
  rect.set_height(rect.height() -
                  GetExpectedSuggestionChipY(kAppListFullscreenProgressValue) -
                  chip_container_rect.height());

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
  // With productivity launcher enabled, add space to the top of the
  // `scrollable_container_` bounds to make room for the gradient mask to be
  // placed above the continue section.
  if (features::IsProductivityLauncherEnabled())
    scrollable_bounds.Inset(
        gfx::Insets::TLBR(-kDefaultFadeoutMaskHeight, 0, 0, 0));
  scrollable_container_->SetBoundsRect(scrollable_bounds);

  if (gradient_layer_delegate_)
    UpdateGradientMaskBounds();

  bool separator_need_centering = false;
  bool first_page_config_changed = false;
  if (features::IsProductivityLauncherEnabled()) {
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
        continue_container_height + toast_container_height +
            GetSeparatorHeight(),
        continue_container_->HasRecentApps());
  }

  // Make sure that UpdateTopLevelGridDimensions() happens after setting the
  // apps grid's first page offset, because it can change the number of rows
  // shown in the grid.
  UpdateTopLevelGridDimensions();

  gfx::Rect apps_grid_bounds(grid_rect.size());
  // Set the apps grid bounds y to make room for the top gradient mask.
  if (features::IsProductivityLauncherEnabled())
    apps_grid_bounds.set_y(kDefaultFadeoutMaskHeight);

  if (apps_grid_view_->bounds() != apps_grid_bounds) {
    apps_grid_view_->SetBoundsRect(apps_grid_bounds);
  } else if (first_page_config_changed) {
    // Apps grid layout depends on the continue container bounds, so explicitly
    // call layout to ensure apps grid view gets laid out even if its bounds do
    // not change.
    apps_grid_view_->Layout();
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
  chip_grid_y_distance_ = scrollable_container_->y() - chip_container_rect.y();

  // Layout page switcher.
  const int page_switcher_width = page_switcher_->GetPreferredSize().width();
  const gfx::Rect page_switcher_bounds(
      grid_rect.right() + kGridToPageSwitcherMargin, grid_rect.y(),
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

const char* AppsContainerView::GetClassName() const {
  return "AppsContainerView";
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

void AppsContainerView::OnThemeChanged() {
  views::View::OnThemeChanged();
  if (separator_) {
    separator_->SetColor(ColorProvider::Get()->GetContentLayerColor(
        ColorProvider::ContentLayerType::kSeparatorColor));
  }
}

void AppsContainerView::OnGestureEvent(ui::GestureEvent* event) {
  // Ignore tap/long-press, allow those to pass to the ancestor view.
  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_LONG_PRESS) {
    return;
  }

  // Will forward events to |apps_grid_view_| if they occur in the same y-region
  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN &&
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

  GetViewAccessibility().OverrideIsLeaf(false);
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
  GetViewAccessibility().OverrideIsLeaf(true);

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
                                                  float search_box_opacity,
                                                  bool restore_opacity) {
  UpdateContainerOpacityForState(state);

  const float progress =
      contents_view_->app_list_view()->GetAppListTransitionProgress(
          AppListView::kProgressFlagNone);
  UpdateContentsOpacity(progress, restore_opacity);
}

void AppsContainerView::UpdatePageBoundsForState(
    AppListState state,
    const gfx::Rect& contents_bounds,
    const gfx::Rect& search_box_bounds) {
  AppListPage::UpdatePageBoundsForState(state, contents_bounds,
                                        search_box_bounds);

  const float progress =
      contents_view_->app_list_view()->GetAppListTransitionProgress(
          AppListView::kProgressFlagNone);
  UpdateContentsYPosition(progress);
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
  const int suggestion_chip_container_size =
      features::IsProductivityLauncherEnabled()
          ? 0
          : kSuggestionChipContainerHeight + kSuggestionChipContainerTopMargin;

  return search_box_size.height() + kAppGridTopMargin +
         suggestion_chip_container_size;
}

int AppsContainerView::GetIdealHorizontalMargin() const {
  if (features::IsProductivityLauncherEnabled())
    return 24;
  const int available_width = GetContentsBounds().width();
  if (available_width >=
      kAppsGridMarginRatio * GetMinHorizontalMarginForAppsGrid()) {
    return available_width / kAppsGridMarginRatio;
  }
  return available_width / kAppsGridMarginRatioForSmallWidth;
}

int AppsContainerView::GetIdealVerticalMargin() const {
  if (!features::IsProductivityLauncherEnabled())
    return GetContentsBounds().height() / kAppsGridMarginRatio;

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

  // For productivity launcher, the `grid_layout`'s rows will be ignored because
  // the vertical margin will be constant.
  const GridLayout grid_layout = CalculateGridLayout();
  const gfx::Size min_grid_size = apps_grid_view()->GetMinimumTileGridSize(
      grid_layout.columns, grid_layout.rows);
  const gfx::Size max_grid_size = apps_grid_view()->GetMaximumTileGridSize(
      grid_layout.columns, grid_layout.rows);

  int available_height = available_bounds.height();
  // Add search box, and suggestion chips container height (with its margins to
  // search box and apps grid) to non apps grid size.
  // NOTE: Not removing bottom apps grid inset because they are included into
  // the total margin values.
  available_height -= GetMinTopMarginForAppsGrid(search_box_size);

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

  int vertical_margin = 0;
  if (features::IsProductivityLauncherEnabled()) {
    // Productivity launcher does not have a preset number of rows per page.
    // Instead of adjusting the margins to fit a set number of rows, the grid
    // will change the number of rows to fit within the provided space.
    vertical_margin = GetIdealVerticalMargin();
  } else {
    vertical_margin =
        calculate_margin(GetIdealVerticalMargin(), available_height,
                         min_grid_size.height(), max_grid_size.height());
  }

  const int horizontal_margin =
      calculate_margin(GetIdealHorizontalMargin(), available_bounds.width(),
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

void AppsContainerView::UpdateRecentApps(bool needs_layout) {
  if (!GetRecentApps() || !app_list_config_)
    return;

  AppListModelProvider* const model_provider = AppListModelProvider::Get();
  GetRecentApps()->SetModels(model_provider->search_model(),
                             model_provider->model());
  if (needs_layout)
    Layout();
}

void AppsContainerView::UpdateSuggestionChips() {
  if (!suggestion_chip_container_view_)
    return;

  suggestion_chip_container_view_->SetResults(
      AppListModelProvider::Get()->search_model()->results());
}

base::ScopedClosureRunner AppsContainerView::DisableSuggestionChipsBlur() {
  if (!suggestion_chip_container_view_)
    return base::ScopedClosureRunner(base::DoNothing());

  ++suggestion_chips_blur_disabler_count_;

  if (suggestion_chips_blur_disabler_count_ == 1)
    suggestion_chip_container_view_->SetBlurDisabled(true);

  return base::ScopedClosureRunner(
      base::BindOnce(&AppsContainerView::OnSuggestionChipsBlurDisablerReleased,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AppsContainerView::SetShowState(ShowState show_state,
                                     bool show_apps_with_animation) {
  if (show_state_ == show_state)
    return;

  show_state_ = show_state;

  // Layout before showing animation because the animation's target bounds are
  // calculated based on the layout.
  Layout();

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

void AppsContainerView::UpdateContentsOpacity(float progress,
                                              bool restore_opacity) {
  apps_grid_view_->UpdateOpacity(restore_opacity, kAppsOpacityChangeStart,
                                 kAppsOpacityChangeEnd);

  // Updates the opacity of page switcher buttons. The same rule as all apps in
  // AppsGridView.
  AppListView* app_list_view = contents_view_->app_list_view();
  int screen_bottom = app_list_view->GetScreenBottom();
  gfx::Rect switcher_bounds = page_switcher_->GetBoundsInScreen();
  float centerline_above_work_area =
      std::max<float>(screen_bottom - switcher_bounds.CenterPoint().y(), 0.f);
  float opacity =
      std::min(std::max((centerline_above_work_area - kAppsOpacityChangeStart) /
                            (kAppsOpacityChangeEnd - kAppsOpacityChangeStart),
                        0.f),
               1.0f);
  page_switcher_->layer()->SetOpacity(restore_opacity ? 1.0f : opacity);

  if (suggestion_chip_container_view_) {
    // Changes the opacity of suggestion chips between 0 and 1 when app list
    // transition progress changes between |kSuggestionChipOpacityStartProgress|
    // and |kSuggestionChipOpacityEndProgress|.
    float chips_opacity =
        base::clamp((progress - kSuggestionChipOpacityStartProgress) /
                        (kSuggestionChipOpacityEndProgress -
                         kSuggestionChipOpacityStartProgress),
                    0.0f, 1.0f);
    suggestion_chip_container_view_->layer()->SetOpacity(
        restore_opacity ? 1.0 : chips_opacity);
  }
}

void AppsContainerView::UpdateContentsYPosition(float progress) {
  const int current_suggestion_chip_y = GetExpectedSuggestionChipY(progress);
  if (suggestion_chip_container_view_)
    suggestion_chip_container_view_->SetY(current_suggestion_chip_y);
  scrollable_container_->SetY(current_suggestion_chip_y +
                              chip_grid_y_distance_);
  page_switcher_->SetY(current_suggestion_chip_y + chip_grid_y_distance_);

  // If app list is in drag, reset transforms that might started animating in
  // AnimateYPosition().
  if (contents_view_->app_list_view()->is_in_drag()) {
    if (suggestion_chip_container_view_)
      suggestion_chip_container_view_->layer()->SetTransform(gfx::Transform());
    scrollable_container_->layer()->SetTransform(gfx::Transform());
    page_switcher_->layer()->SetTransform(gfx::Transform());
  }
}

void AppsContainerView::DisableFocusForShowingActiveFolder(bool disabled) {
  if (suggestion_chip_container_view_) {
    suggestion_chip_container_view_->DisableFocusForShowingActiveFolder(
        disabled);
  }
  if (auto* recent_apps = GetRecentApps(); recent_apps) {
    recent_apps->DisableFocusForShowingActiveFolder(disabled);
  }
  if (auto* continue_section = GetContinueSection(); continue_section) {
    continue_section->DisableFocusForShowingActiveFolder(disabled);
  }
  apps_grid_view_->DisableFocusForShowingActiveFolder(disabled);

  // Ignore the page switcher in accessibility tree so that buttons inside it
  // will not be accessed by ChromeVox.
  SetViewIgnoredForAccessibility(page_switcher_, disabled);
}

int AppsContainerView::GetExpectedSuggestionChipY(float progress) {
  const gfx::Rect search_box_bounds =
      contents_view_->GetSearchBoxExpectedBoundsForProgress(
          AppListState::kStateApps, progress);

  if (!suggestion_chip_container_view_)
    return search_box_bounds.bottom();

  return search_box_bounds.bottom() + kSuggestionChipContainerTopMargin;
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
    preferred_rows = features::IsProductivityLauncherEnabled()
                         ? kPreferredGridRowsInPortraitProductivityLauncher
                         : kPreferredGridColumns;
    preferred_rows_first_page = preferred_rows;
    preferred_columns =
        features::IsProductivityLauncherEnabled()
            ? kPreferredGridColumnsInPortraitProductivityLauncher
            : kPreferredGridRows;
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
  UpdateRecentApps(/*needs_layout=*/false);
  UpdateSuggestionChips();

  // If model changes, close the folder view if it's open, as the associated
  // item list is about to go away.
  SetShowState(SHOW_APPS, false);
}

void AppsContainerView::OnSuggestionChipsBlurDisablerReleased() {
  DCHECK_GT(suggestion_chips_blur_disabler_count_, 0u);
  --suggestion_chips_blur_disabler_count_;

  if (suggestion_chips_blur_disabler_count_ == 0)
    suggestion_chip_container_view_->SetBlurDisabled(false);
}

void AppsContainerView::UpdateGradientMaskBounds() {
  const gfx::Rect container_bounds = scrollable_container_->bounds();
  const gfx::Rect top_gradient_bounds(0, 0, container_bounds.width(),
                                      kDefaultFadeoutMaskHeight);
  const gfx::Rect bottom_gradient_bounds(
      0, container_bounds.height() - kDefaultFadeoutMaskHeight,
      container_bounds.width(), kDefaultFadeoutMaskHeight);

  gradient_layer_delegate_->set_start_fade_zone({top_gradient_bounds,
                                                 /*fade_in=*/true,
                                                 /*is_horizontal=*/false});
  gradient_layer_delegate_->set_end_fade_zone({bottom_gradient_bounds,
                                               /*fade_in=*/false,
                                               /*is_horizonal=*/false});
  gradient_layer_delegate_->layer()->SetBounds(container_bounds);
}

void AppsContainerView::OnAppsGridViewFadeOutAnimationEnded(
    const absl::optional<AppListSortOrder>& new_order,
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
  const bool old_toast_visible = toast_container_->is_toast_visible();

  toast_container_->OnTemporarySortOrderChanged(new_order);
  HandleFocusAfterSort();

  // Skip the fade in animation if the fade out animation is aborted.
  if (abort) {
    OnReorderAnimationEnded();
    return;
  }

  const bool target_toast_visible = toast_container_->is_toast_visible();
  const bool toast_visibility_change =
      (old_toast_visible != target_toast_visible);

  // When the undo toast's visibility changes, the apps grid's bounds should
  // change. Meanwhile, the fade in animation relies on the apps grid's bounds
  // (because of calculating the visible items). Therefore trigger layout before
  // starting the fade in animation.
  if (toast_visibility_change)
    Layout();

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
  if (!contents_view_->app_list_view()->is_tablet_mode())
    return;

  // If the sort is done and the toast is visible, request the focus on the
  // undo button on the toast. Otherwise request the focus on the search box.
  if (toast_container_->is_toast_visible()) {
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

views::View* AppsContainerView::GetShowContinueSectionButtonForTest() {
  return continue_container_
             ? continue_container_->show_continue_section_button()
             : nullptr;
}

}  // namespace ash
