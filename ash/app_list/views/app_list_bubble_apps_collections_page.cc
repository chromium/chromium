// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_apps_collections_page.h"

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "ash/app_list/app_collections_constants.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/apps_collections_controller.h"
#include "ash/app_list/views/app_list_keyboard_controller.h"
#include "ash/app_list/views/app_list_nudge_controller.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/app_list/views/apps_collection_section_view.h"
#include "ash/app_list/views/apps_collections_dismiss_dialog.h"
#include "ash/app_list/views/apps_grid_context_menu.h"
#include "ash/app_list/views/search_result_page_dialog_controller.h"
#include "ash/controls/rounded_scroll_bar.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

// This ID is from //chrome/browser/web_applications/web_app_id_constants.h.
constexpr char kHelpAppId[] = "nbljnnecbjbmifnoehiemkgefbnpoeak";

// Insets for the vertical scroll bar. The bottom is pushed up slightly to keep
// the scroll bar from being clipped by the rounded corners.
constexpr auto kVerticalScrollInsets = gfx::Insets::TLBR(1, 0, 16, 1);

// The padding between different sections within the apps collections page. Also
// used for interior page container margin.
constexpr int kVerticalPaddingBetweenSections = 8;
constexpr int kVerticalPaddingBetweenNudgeAndSections = 8;

// The horizontal interior margin for the apps page container - i.e. the margin
// between the page bounds and the page content.
constexpr int kHorizontalInteriorMargin = 16;

// TODO(anasalazar): Update the animation details when a motion spec is set.
// Right now we are using the same transition as the apps page. The spec says
// "Down 40 -> 0, duration 250ms" with no delay, but the opacity animation has a
// 50ms delay that causes the first 50ms to be invisible. Just animate the 200ms
// visible part, which is 32 dips. This ensures the search page hide animation
// doesn't play at the same time as the apps page show animation.
constexpr int kShowPageAnimationVerticalOffset = 32;
constexpr base::TimeDelta kShowPageAnimationTransformDuration =
    base::Milliseconds(200);

// Delay for the show page transform and opacity animations.
constexpr base::TimeDelta kShowPageAnimationDelay = base::Milliseconds(50);

// Duration of the show page opacity animation.
constexpr base::TimeDelta kShowPageAnimationOpacityDuration =
    base::Milliseconds(100);

}  // namespace

AppListBubbleAppsCollectionsPage::AppListBubbleAppsCollectionsPage(
    AppListViewDelegate* view_delegate,
    AppListConfig* app_list_config,
    AppListA11yAnnouncer* a11y_announcer,
    SearchResultPageDialogController* dialog_controller,
    base::OnceClosure exit_page_callback)
    : view_delegate_(view_delegate),
      app_list_config_(app_list_config),
      dialog_controller_(dialog_controller),
      app_list_nudge_controller_(std::make_unique<AppListNudgeController>()),
      exit_page_callback_(std::move(exit_page_callback)) {
  AppListModelProvider::Get()->AddObserver(this);
  SetUseDefaultFillLayout(true);

  // The entire page scrolls.
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll_view_->SetDrawOverflowIndicator(false);
  // Don't paint a background. The bubble already has one.
  scroll_view_->SetBackgroundColor(std::nullopt);
  // Arrow keys are used to select app icons.
  scroll_view_->SetAllowKeyboardScrolling(false);

  // Scroll view will have a gradient mask layer, and is animated during
  // hide/show.
  scroll_view_->SetPaintToLayer();
  scroll_view_->layer()->SetFillsBoundsOpaquely(false);

  // Set up scroll bars.
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  auto vertical_scroll = std::make_unique<RoundedScrollBar>(
      RoundedScrollBar::Orientation::kVertical);
  vertical_scroll->SetInsets(kVerticalScrollInsets);
  vertical_scroll->SetSnapBackOnDragOutside(false);
  scroll_bar_ = vertical_scroll.get();
  scroll_view_->SetVerticalScrollBar(std::move(vertical_scroll));

  auto scroll_contents = std::make_unique<views::View>();
  auto* layout =
      scroll_contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          gfx::Insets::VH(kVerticalPaddingBetweenNudgeAndSections,
                          kHorizontalInteriorMargin),
          kVerticalPaddingBetweenNudgeAndSections));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  // Add a empty container view. A toast view should be added to
  // `toast_container_` for user ed.
  toast_container_ =
      scroll_contents->AddChildView(std::make_unique<AppListToastContainerView>(
          app_list_nudge_controller_.get(), /*keyboard_controller=*/nullptr,
          a11y_announcer, view_delegate,
          /*delegate=*/this,
          /*tablet_mode=*/false));

  AppListModel* const model = AppListModelProvider::Get()->model();

  sections_container_ =
      scroll_contents->AddChildView(std::make_unique<views::View>());
  sections_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(kVerticalPaddingBetweenSections, 0),
      kVerticalPaddingBetweenSections));

  PopulateCollections(model);

  views::BoxLayoutView* discovery_chip_container =
      scroll_contents->AddChildView(std::make_unique<views::BoxLayoutView>());
  discovery_chip_container->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  discovery_chip_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  discovery_chip_ =
      discovery_chip_container->AddChildView(std::make_unique<ash::PillButton>(
          base::BindRepeating(
              &AppListBubbleAppsCollectionsPage::OnDiscoveryChipPressed,
              base::Unretained(this)),
          l10n_util::GetStringUTF16(
              IDS_ASH_LAUNCHER_APPS_COLLECTIONS_DISCOVERY_CHIP_LABEL),
          ash::PillButton::kDefaultWithIconLeading, &kDiscoveryChipIcon));

  scroll_view_->SetContents(std::move(scroll_contents));
  toast_container_->CreateTutorialNudgeView();
  toast_container_->UpdateVisibilityState(
      AppListToastContainerView::VisibilityState::kShown);

  context_menu_ =
      std::make_unique<AppsGridContextMenu>(GetGridTypeForContextMenu());
  set_context_menu_controller(context_menu_.get());

  on_contents_scrolled_subscription_ =
      scroll_view_->AddContentsScrolledCallback(
          base::BindRepeating(&AppListBubbleAppsCollectionsPage::OnPageScrolled,
                              base::Unretained(this)));

  AppsCollectionsController::Get()->SetReorderCallback(
      base::BindRepeating(&AppListBubbleAppsCollectionsPage::RequestAppReorder,
                          weak_factory_.GetWeakPtr()));
}

AppListBubbleAppsCollectionsPage::~AppListBubbleAppsCollectionsPage() {
  AppListModelProvider::Get()->RemoveObserver(this);
}

void AppListBubbleAppsCollectionsPage::OnDiscoveryChipPressed() {
  view_delegate_->ActivateItem(
      kHelpAppId,
      /*event_flags=*/0, ash::AppListLaunchedFrom::kLaunchedFromDiscoveryChip,
      /*is_app_above_the_fold=*/false);
}

void AppListBubbleAppsCollectionsPage::AnimateShowPage() {
  // If skipping animations, just update visibility.
  if (ui::ScopedAnimationDurationScaleMode::is_zero()) {
    SetVisible(true);
    return;
  }

  // Ensure any in-progress animations have their cleanup callbacks called.
  // Note that this might call SetVisible(false) from the hide animation.
  AbortAllAnimations();

  // Ensure the view is visible.
  SetVisible(true);

  ui::Layer* scroll_view_layer = scroll_view_->layer();
  DCHECK(scroll_view_layer);
  DCHECK_EQ(scroll_view_layer->type(), ui::LAYER_TEXTURED);

  gfx::Transform translate_down;
  translate_down.Translate(0, kShowPageAnimationVerticalOffset);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindRepeating(
          &AppListBubbleAppsCollectionsPage::SetVisibilityAfterAnimation,
          weak_factory_.GetWeakPtr(), /* visible= */ true))
      .OnAborted(base::BindRepeating(
          &AppListBubbleAppsCollectionsPage::SetVisibilityAfterAnimation,
          weak_factory_.GetWeakPtr(), /* visible= */ true))
      .Once()
      .SetOpacity(scroll_view_layer, 0.f)
      .SetTransform(scroll_view_layer, translate_down)
      .At(kShowPageAnimationDelay)
      .SetDuration(kShowPageAnimationTransformDuration)
      .SetTransform(scroll_view_layer, gfx::Transform(),
                    gfx::Tween::LINEAR_OUT_SLOW_IN)
      .At(kShowPageAnimationDelay)
      .SetDuration(kShowPageAnimationOpacityDuration)
      .SetOpacity(scroll_view_layer, 1.f);
}

void AppListBubbleAppsCollectionsPage::AnimateHidePage() {
  // If skipping animations, just update visibility.
  if (ui::ScopedAnimationDurationScaleMode::is_zero()) {
    SetVisible(false);
    return;
  }

  ui::Layer* scroll_view_layer = scroll_view_->layer();
  DCHECK(scroll_view_layer);
  DCHECK_EQ(scroll_view_layer->type(), ui::LAYER_TEXTURED);

  // The animation spec says 40 dips down over 250ms, but the opacity animation
  // renders the view invisible after 50ms, so animate the visible fraction.
  gfx::Transform translate_down;
  constexpr int kVerticalOffset = 40 * 50 / 250;
  translate_down.Translate(0, kVerticalOffset);

  // Opacity: 100% -> 0%, duration 50ms
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindRepeating(
          &AppListBubbleAppsCollectionsPage::SetVisibilityAfterAnimation,
          weak_factory_.GetWeakPtr(), /* visible= */ false))
      .OnAborted(base::BindRepeating(
          &AppListBubbleAppsCollectionsPage::SetVisibilityAfterAnimation,
          weak_factory_.GetWeakPtr(), /* visible= */ false))
      .Once()
      .SetDuration(base::Milliseconds(50))
      .SetOpacity(scroll_view_layer, 0.f)
      .SetTransform(scroll_view_layer, translate_down);
}

void AppListBubbleAppsCollectionsPage::AbortAllAnimations() {
  auto abort_animations = [](views::View* view) {
    if (view->layer()) {
      view->layer()->GetAnimator()->AbortAllAnimations();
    }
  };
  abort_animations(scroll_view_);
  if (toast_container_) {
    abort_animations(toast_container_);
  }
  abort_animations(sections_container_);
}

void AppListBubbleAppsCollectionsPage::OnNudgeRemoved() {
  AppsCollectionsController::Get()->SetAppsCollectionDismissed(
      AppsCollectionsController::DismissReason::kExitNudge);

  CHECK(exit_page_callback_);

  std::move(exit_page_callback_).Run();
}

AppsGridContextMenu::GridType
AppListBubbleAppsCollectionsPage::GetGridTypeForContextMenu() {
  return AppsGridContextMenu::GridType::kAppsCollectionsGrid;
}

ui::Layer* AppListBubbleAppsCollectionsPage::GetPageAnimationLayerForTest() {
  // Animating the `scroll_view_`'s content layer can have its transform
  // animations interrupted when the content layer's transforms get set due to
  // rtl specific transforms in ScrollView code. Use the `scroll_view_` layer
  // for animations to avoid this.
  return scroll_view_->layer();
}

AppListToastContainerView*
AppListBubbleAppsCollectionsPage::GetToastContainerViewForTest() {
  return toast_container_;
}

void AppListBubbleAppsCollectionsPage::SetVisibilityAfterAnimation(
    bool visible) {
  // Ensure the view has the correct opacity and transform when the animation is
  // aborted.
  SetVisible(visible);
  ui::Layer* layer = scroll_view()->layer();
  layer->SetOpacity(1.f);
  layer->SetTransform(gfx::Transform());
}

void AppListBubbleAppsCollectionsPage::OnActiveAppListModelsChanged(
    AppListModel* model,
    SearchModel* search_model) {
  PopulateCollections(model);
}

bool AppListBubbleAppsCollectionsPage::IsInFolder() const {
  return false;
}

void AppListBubbleAppsCollectionsPage::SetSelectedView(AppListItemView* view) {
  selected_view_ = view;
}

void AppListBubbleAppsCollectionsPage::ClearSelectedView() {
  selected_view_ = nullptr;
}

bool AppListBubbleAppsCollectionsPage::IsSelectedView(
    const AppListItemView* view) const {
  return view == selected_view_;
}

void AppListBubbleAppsCollectionsPage::EndDrag(bool cancel) {}

void AppListBubbleAppsCollectionsPage::OnAppListItemViewActivated(
    AppListItemView* pressed_item_view,
    const ui::Event& event) {
  const std::string id = pressed_item_view->item()->id();
  view_delegate_->ActivateItem(
      id, event.flags(), AppListLaunchedFrom::kLaunchedFromAppsCollections,
      IsAboveTheFold(pressed_item_view));
  RecordAppListByCollectionLaunched(pressed_item_view->item()->collection_id(),
                                    /*is_apps_collections_page=*/true);
  // `this` may be deleted.
}

bool AppListBubbleAppsCollectionsPage::IsAboveTheFold(
    AppListItemView* item_view) {
  gfx::Rect item_bounds_in_scroll_view = views::View::ConvertRectToTarget(
      item_view, scroll_view_->contents(), item_view->GetLocalBounds());
  return item_bounds_in_scroll_view.bottom() <
         scroll_view_->GetVisibleRect().height();
}

void AppListBubbleAppsCollectionsPage::SetDialogController(
    SearchResultPageDialogController* dialog_controller) {
  dialog_controller_ = dialog_controller;
}

void AppListBubbleAppsCollectionsPage::RecordAboveTheFoldMetrics() {
  std::vector<std::string> apps_above_the_fold = {};
  std::vector<std::string> apps_below_the_fold = {};
  for (views::View* child_view : sections_container_->children()) {
    AppsCollectionSectionView* collection_view =
        views::AsViewClass<AppsCollectionSectionView>(child_view);
    for (size_t i = 0; i < collection_view->item_views()->view_size(); ++i) {
      AppListItemView* app_view = collection_view->item_views()->view_at(i);
      if (IsAboveTheFold(app_view)) {
        apps_above_the_fold.push_back(app_view->item()->id());
      } else {
        apps_below_the_fold.push_back(app_view->item()->id());
      }
    }
  }
  view_delegate_->RecordAppsDefaultVisibility(
      apps_above_the_fold, apps_below_the_fold,
      /*is_apps_collections_page=*/true);
}

void AppListBubbleAppsCollectionsPage::PopulateCollections(
    AppListModel* model) {
  sections_container_->RemoveAllChildViews();
  if (!model) {
    return;
  }

  std::vector<AppCollection> available_collections = GetAppCollections();
  for (AppCollection collection : available_collections) {
    AppsCollectionSectionView* collection_view =
        sections_container_->AddChildView(
            std::make_unique<AppsCollectionSectionView>(collection,
                                                        view_delegate_, this));
    collection_view->UpdateAppListConfig(app_list_config_);
    collection_view->SetModel(model);
  }
}

void AppListBubbleAppsCollectionsPage::RequestAppReorder(
    AppListSortOrder order) {
  CHECK(dialog_controller_);

  std::unique_ptr<views::WidgetDelegate> dialog =
      std::make_unique<AppsCollectionsDismissDialog>(base::BindOnce(
          &AppListBubbleAppsCollectionsPage::DismissPageAndReorder,
          weak_factory_.GetWeakPtr(), order));
  dialog_controller_->Show(std::move(dialog));
}

void AppListBubbleAppsCollectionsPage::DismissPageAndReorder(
    AppListSortOrder order) {
  AppListModelProvider::Get()->model()->delegate()->RequestAppListSort(order);

  AppsCollectionsController::Get()->SetAppsCollectionDismissed(
      AppsCollectionsController::DismissReason::kSorting);

  CHECK(exit_page_callback_);

  std::move(exit_page_callback_).Run();
}

void AppListBubbleAppsCollectionsPage::OnPageScrolled() {
  const gfx::Rect visible_bounds = scroll_view_->GetVisibleRect();
  const gfx::Rect contents_bounds = scroll_view_->contents()->bounds();

  // Do not log anything if the contents are not scrolled.
  if (visible_bounds.y() == contents_bounds.y()) {
    last_bottom_scroll_offset_.reset();
    return;
  }

  const int bottom_scroll_offset =
      contents_bounds.bottom() - visible_bounds.bottom();
  const int buffer =
      kVerticalPaddingBetweenSections + kVerticalPaddingBetweenNudgeAndSections;
  if (bottom_scroll_offset <= buffer &&
      (!last_bottom_scroll_offset_ || last_bottom_scroll_offset_ > buffer)) {
    RecordLauncherWorkflowMetrics(
        AppListUserAction::kNavigatedToBottomOfAppList,
        /*is_tablet_mode = */ false, std::nullopt);
  }
  last_bottom_scroll_offset_ = bottom_scroll_offset;
}

BEGIN_METADATA(AppListBubbleAppsCollectionsPage)
END_METADATA

}  // namespace ash
