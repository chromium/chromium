// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_page_view.h"

#include <stddef.h>

#include <algorithm>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/privacy_container_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_page_anchored_dialog.h"
#include "ash/app_list/views/search_result_tile_item_list_view.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/view_shadow.h"
#include "ash/search_box/search_box_constants.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

namespace {

constexpr int kHeight = 440;
constexpr int kWidth = 640;

// The horizontal padding of the separator.
constexpr int kSeparatorPadding = 12;
constexpr int kSeparatorThickness = 1;

// The height of the search box in this page.
constexpr int kSearchBoxHeight = 56;

// The spacing between search box bottom and separator line.
// Add 1 pixel spacing so that the search bbox bottom will not paint over
// the separator line drawn by SearchResultPageBackground in some scale factors
// due to the round up.
constexpr int kSearchBoxBottomSpacing = 1;

// Minimum spacing between shelf and bottom of search box.
constexpr int kSearchResultPageMinimumBottomMargin = 24;

// The shadow elevation value for the shadow of the expanded search box.
constexpr int kSearchBoxSearchResultShadowElevation = 12;

// The amount of time by which notifications to accessibility framework about
// result page changes are delayed.
constexpr base::TimeDelta kNotifyA11yDelay =
    base::TimeDelta::FromMilliseconds(500);

// A container view that ensures the card background and the shadow are painted
// in the correct order.
class SearchCardView : public views::View {
 public:
  METADATA_HEADER(SearchCardView);
  explicit SearchCardView(std::unique_ptr<views::View> content_view) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(std::move(content_view));
  }
  SearchCardView(const SearchCardView&) = delete;
  SearchCardView& operator=(const SearchCardView&) = delete;
  ~SearchCardView() override {}
};

BEGIN_METADATA(SearchCardView, views::View)
END_METADATA

class ZeroWidthVerticalScrollBar : public views::OverlayScrollBar {
 public:
  ZeroWidthVerticalScrollBar() : OverlayScrollBar(false) {}
  ZeroWidthVerticalScrollBar(const ZeroWidthVerticalScrollBar&) = delete;
  ZeroWidthVerticalScrollBar& operator=(const ZeroWidthVerticalScrollBar&) =
      delete;
  ~ZeroWidthVerticalScrollBar() override = default;

  // OverlayScrollBar overrides:
  int GetThickness() const override { return 0; }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    // Arrow keys should be handled by FocusManager to move focus. When a search
    // result is focused, it will be set visible in scroll view.
    return false;
  }
};

class SearchResultPageBackground : public views::Background {
 public:
  explicit SearchResultPageBackground(SkColor color) {
    SetNativeControlColor(color);
  }
  SearchResultPageBackground(const SearchResultPageBackground&) = delete;
  SearchResultPageBackground& operator=(const SearchResultPageBackground&) =
      delete;
  ~SearchResultPageBackground() override = default;

 private:
  // views::Background overrides:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    canvas->DrawColor(get_color());
    gfx::Rect bounds = view->GetContentsBounds();
    if (bounds.height() <= kSearchBoxHeight)
      return;
    // Draw a separator between SearchBoxView and SearchResultPageView.
    bounds.set_y(kSearchBoxHeight + kSearchBoxBottomSpacing);
    bounds.set_height(kSeparatorThickness);
    canvas->FillRect(bounds, AppListColorProvider::Get()->GetSeparatorColor());
  }
};

}  // namespace

class SearchResultPageView::HorizontalSeparator : public views::View {
 public:
  explicit HorizontalSeparator(int preferred_width)
      : preferred_width_(preferred_width) {
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets(0, kSeparatorPadding, 0, kSeparatorPadding)));
  }

  ~HorizontalSeparator() override {}

  // views::View overrides:
  const char* GetClassName() const override { return "HorizontalSeparator"; }

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(preferred_width_, kSeparatorThickness);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    gfx::Rect rect = GetContentsBounds();
    canvas->FillRect(rect, AppListColorProvider::Get()->GetSeparatorColor());
    View::OnPaint(canvas);
  }

 private:
  const int preferred_width_;

  DISALLOW_COPY_AND_ASSIGN(HorizontalSeparator);
};

SearchResultPageView::SearchResultPageView(SearchModel* search_model)
    : search_model_(search_model), contents_view_(new views::View) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  contents_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  view_shadow_ =
      std::make_unique<ViewShadow>(this, kSearchBoxSearchResultShadowElevation);
  view_shadow_->SetRoundedCornerRadius(
      kSearchBoxBorderCornerRadiusSearchResult);

  // Hides this view behind the search box by using the same color and
  // background border corner radius. All child views' background should be
  // set transparent so that the rounded corner is not overwritten.
  SetBackground(std::make_unique<SearchResultPageBackground>(
      AppListColorProvider::Get()->GetSearchBoxCardBackgroundColor()));
  auto scroller = std::make_unique<views::ScrollView>();
  // Leaves a placeholder area for the search box and the separator below it.
  scroller->SetBorder(views::CreateEmptyBorder(gfx::Insets(
      kSearchBoxHeight + kSearchBoxBottomSpacing + kSeparatorThickness, 0, 0,
      0)));
  scroller->SetDrawOverflowIndicator(false);
  scroller->SetContents(base::WrapUnique(contents_view_));
  // Setting clip height is necessary to make ScrollView take into account its
  // contents' size. Using zeroes doesn't prevent it from scrolling and sizing
  // correctly.
  scroller->ClipHeightTo(0, 0);
  scroller->SetVerticalScrollBar(
      std::make_unique<ZeroWidthVerticalScrollBar>());
  scroller->SetBackgroundColor(base::nullopt);
  AddChildView(std::move(scroller));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  result_selection_controller_ = std::make_unique<ResultSelectionController>(
      &result_container_views_,
      base::BindRepeating(&SearchResultPageView::SelectedResultChanged,
                          base::Unretained(this)));

  search_box_observer_.Add(search_model->search_box());
}

SearchResultPageView::~SearchResultPageView() = default;

void SearchResultPageView::AddSearchResultContainerViewInternal(
    std::unique_ptr<SearchResultContainerView> result_container) {
  if (!result_container_views_.empty()) {
    separators_.push_back(contents_view_->AddChildView(
        std::make_unique<HorizontalSeparator>(bounds().width())));
  }
  auto* result_container_ptr = result_container.get();
  contents_view_->AddChildView(
      std::make_unique<SearchCardView>(std::move(result_container)));
  result_container_views_.push_back(result_container_ptr);
  result_container_ptr->SetResults(search_model_->results());
  result_container_ptr->set_delegate(this);
}

bool SearchResultPageView::IsFirstResultTile() const {
  // In the event that the result does not exist, it is not a tile.
  if (!first_result_view_ || !first_result_view_->result())
    return false;

  return first_result_view_->result()->display_type() ==
         SearchResultDisplayType::kTile;
}

bool SearchResultPageView::IsFirstResultHighlighted() const {
  DCHECK(first_result_view_);
  return first_result_view_->selected();
}

const char* SearchResultPageView::GetClassName() const {
  return "SearchResultPageView";
}

gfx::Size SearchResultPageView::CalculatePreferredSize() const {
  return gfx::Size(kWidth, kHeight);
}

void SearchResultPageView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // The clip rect set for page state animations needs to be reset when the
  // bounds change because page size change invalidates the previous bounds.
  // This allows content to properly follow target bounds when screen rotates.
  if (previous_bounds.size() != bounds().size())
    layer()->SetClipRect(gfx::Rect());
}

void SearchResultPageView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (!GetVisible())
    return;

  node_data->role = ax::mojom::Role::kListBox;

  base::string16 value;
  base::string16 query = search_model_->search_box()->text();
  if (!query.empty()) {
    if (last_search_result_count_ == 1) {
      value = l10n_util::GetStringFUTF16(
          IDS_APP_LIST_SEARCHBOX_RESULTS_ACCESSIBILITY_ANNOUNCEMENT_SINGLE_RESULT,
          query);
    } else {
      value = l10n_util::GetStringFUTF16(
          IDS_APP_LIST_SEARCHBOX_RESULTS_ACCESSIBILITY_ANNOUNCEMENT,
          base::NumberToString16(last_search_result_count_), query);
    }
  } else {
    value = l10n_util::GetStringUTF16(
        IDS_APP_LIST_SEARCHBOX_RESULTS_ACCESSIBILITY_ANNOUNCEMENT_ZERO_STATE);
  }

  node_data->SetValue(value);
}

void SearchResultPageView::ReorderSearchResultContainers() {
  // Sort the result container views by their score.
  std::sort(result_container_views_.begin(), result_container_views_.end(),
            [](const SearchResultContainerView* a,
               const SearchResultContainerView* b) -> bool {
              return a->container_score() > b->container_score();
            });

  for (size_t i = 0; i < result_container_views_.size(); ++i) {
    SearchResultContainerView* view = result_container_views_[i];

    if (i > 0) {
      HorizontalSeparator* separator = separators_[i - 1];
      const bool preceded_by_privacy_container =
          result_container_views_[i - 1] ==
          AppListPage::contents_view()->privacy_container_view();
      // Hides the separator above the container that has no results, and below
      // the privacy container.
      separator->SetVisible(view->num_results() &&
                            !preceded_by_privacy_container);

      contents_view_->ReorderChildView(separator, i * 2 - 1);
      contents_view_->ReorderChildView(view->parent(), i * 2);
    } else {
      contents_view_->ReorderChildView(view->parent(), i);
    }
  }

  Layout();
}

void SearchResultPageView::SelectedResultChanged() {
  if (!result_selection_controller_->selected_location_details() ||
      !result_selection_controller_->selected_result()) {
    return;
  }

  const ResultLocationDetails* selection_details =
      result_selection_controller_->selected_location_details();
  views::View* selected_row = nullptr;
  // For horizontal containers ensure that the whole container fits in the
  // scroll view, to account for vertical padding within the container.
  if (selection_details->container_is_horizontal) {
    selected_row = result_container_views_[selection_details->container_index];
  } else {
    selected_row = result_selection_controller_->selected_result();
  }

  selected_row->ScrollViewToVisible();

  NotifySelectedResultChanged();
}

void SearchResultPageView::SetIgnoreResultChangesForA11y(bool ignore) {
  if (ignore_result_changes_for_a11y_ == ignore)
    return;
  ignore_result_changes_for_a11y_ = ignore;

  GetViewAccessibility().OverrideIsLeaf(ignore);
  NotifyAccessibilityEvent(ax::mojom::Event::kTreeChanged, true);
}

void SearchResultPageView::ScheduleResultsChangedA11yNotification() {
  if (!ignore_result_changes_for_a11y_) {
    NotifyA11yResultsChanged();
    return;
  }

  notify_a11y_results_changed_timer_.Start(
      FROM_HERE, kNotifyA11yDelay,
      base::BindOnce(&SearchResultPageView::NotifyA11yResultsChanged,
                     base::Unretained(this)));
}

void SearchResultPageView::NotifyA11yResultsChanged() {
  SetIgnoreResultChangesForA11y(false);

  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
  NotifySelectedResultChanged();
}

void SearchResultPageView::NotifySelectedResultChanged() {
  if (ignore_result_changes_for_a11y_ ||
      !result_selection_controller_->selected_location_details() ||
      !result_selection_controller_->selected_result()) {
    return;
  }

  NotifyAccessibilityEvent(ax::mojom::Event::kSelectedChildrenChanged, true);
  result_selection_controller_->selected_result()->NotifyA11yResultSelected();
}

void SearchResultPageView::OnSearchResultContainerResultsChanging() {
  // Block any result selection changes while result updates are in flight.
  // The selection will be reset once the results are all updated.
  result_selection_controller_->set_block_selection_changes(true);

  notify_a11y_results_changed_timer_.Stop();
  SetIgnoreResultChangesForA11y(true);
}

void SearchResultPageView::OnSearchResultContainerResultsChanged() {
  DCHECK(!result_container_views_.empty());
  DCHECK(result_container_views_.size() == separators_.size() + 1);

  int result_count = 0;
  // Only sort and layout the containers when they have all updated.
  for (SearchResultContainerView* view : result_container_views_) {
    if (view->UpdateScheduled())
      return;
    result_count += view->num_results();
  }

  last_search_result_count_ = result_count;

  ReorderSearchResultContainers();

  ScheduleResultsChangedA11yNotification();

  first_result_view_ = result_container_views_[0]->GetFirstResultView();

  // Reset selection to first when things change. The first result is set as
  // as the default result.
  result_selection_controller_->set_block_selection_changes(false);
  result_selection_controller_->ResetSelection(nullptr /*key_event*/,
                                               true /* default_selection */);
  // Update SearchBoxView search box autocomplete as necessary based on new
  // first result view.
  AppListPage::contents_view()->GetSearchBoxView()->ProcessAutocomplete();
}

void SearchResultPageView::Update() {
  notify_a11y_results_changed_timer_.Stop();
}

void SearchResultPageView::SearchEngineChanged() {}

void SearchResultPageView::ShowAssistantChanged() {}

void SearchResultPageView::ShowAnchoredDialog(
    std::unique_ptr<views::DialogDelegateView> dialog) {
  ContentsView* const contents_view = AppListPage::contents_view();
  if (contents_view->GetActiveState() != AppListState::kStateSearchResults)
    return;

  anchored_dialog_ = std::make_unique<SearchResultPageAnchoredDialog>(
      std::move(dialog), contents_view,
      base::BindOnce(&SearchResultPageView::OnAnchoredDialogClosed,
                     base::Unretained(this)));
  const gfx::Rect anchor_bounds =
      contents_view->GetSearchBoxBounds(AppListState::kStateSearchResults);
  anchored_dialog_->UpdateBounds(anchor_bounds);

  anchored_dialog_->widget()->Show();
}

void SearchResultPageView::OnWillBeHidden() {
  anchored_dialog_.reset();
}

void SearchResultPageView::OnHidden() {
  // Hide the search results page when it is behind search box to avoid focus
  // being moved onto suggested apps when zero state is enabled.
  AppListPage::OnHidden();
  notify_a11y_results_changed_timer_.Stop();
  SetVisible(false);
  for (auto* container_view : result_container_views_) {
    container_view->SetShown(false);
  }
}

void SearchResultPageView::OnShown() {
  AppListPage::OnShown();
  for (auto* container_view : result_container_views_) {
    container_view->SetShown(true);
  }
  ScheduleResultsChangedA11yNotification();
}

void SearchResultPageView::AnimateYPosition(AppListViewState target_view_state,
                                            const TransformAnimator& animator,
                                            float default_offset) {
  // Search result page view may host a native view to show answer card results.
  // The native view hosts use view to widget coordinate conversion to calculate
  // the native view bounds, and thus depend on the view transform values.
  // Make sure the view is laid out before starting the transform animation so
  // native views are not placed according to interim, animated page transform
  // value.
  layer()->GetAnimator()->StopAnimatingProperty(
      ui::LayerAnimationElement::TRANSFORM);
  if (needs_layout())
    Layout();

  animator.Run(default_offset, layer(), this);
  animator.Run(default_offset, view_shadow_->shadow()->shadow_layer(), nullptr);
  if (anchored_dialog_) {
    const float offset =
        anchored_dialog_->AdjustVerticalTransformOffset(default_offset);
    animator.Run(offset, anchored_dialog_->widget()->GetLayer(), nullptr);
  }
}

void SearchResultPageView::UpdatePageOpacityForState(AppListState state,
                                                     float search_box_opacity,
                                                     bool restore_opacity) {
  layer()->SetOpacity(search_box_opacity);
}

void SearchResultPageView::UpdatePageBoundsForState(
    AppListState state,
    const gfx::Rect& contents_bounds,
    const gfx::Rect& search_box_bounds) {
  AppListPage::UpdatePageBoundsForState(state, contents_bounds,
                                        search_box_bounds);
  if (anchored_dialog_)
    anchored_dialog_->UpdateBounds(search_box_bounds);
}

gfx::Rect SearchResultPageView::GetPageBoundsForState(
    AppListState state,
    const gfx::Rect& contents_bounds,
    const gfx::Rect& search_box_bounds) const {
  if (state != AppListState::kStateSearchResults) {
    // Hides this view behind the search box by using the same bounds.
    return search_box_bounds;
  }

  gfx::Rect bounding_rect = contents_bounds;
  bounding_rect.Inset(0, 0, 0, kSearchResultPageMinimumBottomMargin);

  gfx::Rect preferred_bounds =
      gfx::Rect(search_box_bounds.origin(),
                gfx::Size(search_box_bounds.width(), kHeight));
  preferred_bounds.Intersect(bounding_rect);

  return preferred_bounds;
}

void SearchResultPageView::OnAnimationStarted(AppListState from_state,
                                              AppListState to_state) {
  if (from_state != AppListState::kStateSearchResults &&
      to_state != AppListState::kStateSearchResults) {
    return;
  }

  const ContentsView* const contents_view = AppListPage::contents_view();
  const gfx::Rect contents_bounds = contents_view->GetContentsBounds();
  const gfx::Rect from_rect =
      GetPageBoundsForState(from_state, contents_bounds,
                            contents_view->GetSearchBoxBounds(from_state));
  const gfx::Rect to_rect = GetPageBoundsForState(
      to_state, contents_bounds, contents_view->GetSearchBoxBounds(to_state));
  if (from_rect == to_rect)
    return;

  const int to_radius =
      contents_view->GetSearchBoxView()->GetSearchBoxBorderCornerRadiusForState(
          to_state);

  // Here does the following animations;
  // - clip-rect, so it looks like expanding from |from_rect| to |to_rect|.
  // - rounded-rect
  // - transform of the shadow
  SetBoundsRect(to_rect);
  gfx::Rect clip_rect = from_rect;
  clip_rect -= to_rect.OffsetFromOrigin();
  layer()->SetClipRect(clip_rect);
  {
    auto settings = contents_view->CreateTransitionAnimationSettings(layer());
    layer()->SetClipRect(gfx::Rect(to_rect.size()));
    // This changes the shadow's corner immediately while this corner bounds
    // gradually. This would be fine because this would be unnoticeable to
    // users.
    view_shadow_->SetRoundedCornerRadius(to_radius);
  }

  // Animate the shadow's bounds through transform.
  {
    gfx::Transform transform;
    transform.Translate(from_rect.origin() - to_rect.origin());
    transform.Scale(static_cast<float>(from_rect.width()) / to_rect.width(),
                    static_cast<float>(from_rect.height()) / to_rect.height());
    view_shadow_->shadow()->layer()->SetTransform(transform);

    auto settings = contents_view->CreateTransitionAnimationSettings(
        view_shadow_->shadow()->layer());
    view_shadow_->shadow()->layer()->SetTransform(gfx::Transform());
  }
}

void SearchResultPageView::OnAnimationUpdated(double progress,
                                              AppListState from_state,
                                              AppListState to_state) {
  if (from_state != AppListState::kStateSearchResults &&
      to_state != AppListState::kStateSearchResults) {
    return;
  }
  const SearchBoxView* search_box =
      AppListPage::contents_view()->GetSearchBoxView();
  const SkColor color = gfx::Tween::ColorValueBetween(
      progress, search_box->GetBackgroundColorForState(from_state),
      search_box->GetBackgroundColorForState(to_state));

  if (color != background()->get_color()) {
    background()->SetNativeControlColor(color);
    SchedulePaint();
  }
}

gfx::Size SearchResultPageView::GetPreferredSearchBoxSize() const {
  static gfx::Size size = gfx::Size(kWidth, kSearchBoxHeight);
  return size;
}

base::Optional<int> SearchResultPageView::GetSearchBoxTop(
    AppListViewState view_state) const {
  if (view_state == AppListViewState::kPeeking ||
      view_state == AppListViewState::kHalf) {
    return AppListConfig::instance().search_box_fullscreen_top_padding();
  }
  // For other view states, return base::nullopt so the ContentsView
  // sets the default search box widget origin.
  return base::nullopt;
}

views::View* SearchResultPageView::GetFirstFocusableView() {
  return GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), false /* reverse */, false /* dont_loop */);
}

views::View* SearchResultPageView::GetLastFocusableView() {
  return GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), true /* reverse */, false /* dont_loop */);
}

void SearchResultPageView::OnAnchoredDialogClosed() {
  anchored_dialog_.reset();
}

}  // namespace ash
