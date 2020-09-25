// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_page_view.h"

#include <algorithm>
#include <utility>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/assistant/assistant_main_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/view_shadow.h"
#include "ash/search_box/search_box_constants.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/views/background.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/layout_manager_base.h"

namespace ash {

namespace {

// The min/max height of this page.
constexpr int kMaxHeightDip = 440;
constexpr int kMinHeightDip = 180;

// The height of the search box in this page.
constexpr int kSearchBoxHeightDip = 56;

// The shadow elevation value for the shadow of the Assistant search box.
constexpr int kShadowElevation = 12;

// Helpers ---------------------------------------------------------------------

int GetPreferredHeightForAppListState(AppListView* app_list_view) {
  auto app_list_view_state = app_list_view->app_list_state();
  switch (app_list_view_state) {
    case AppListViewState::kHalf:
    case AppListViewState::kFullscreenSearch:
      return kMaxHeightDip;
    default:
      return kMinHeightDip;
  }
}

// AssistantPageViewLayout -----------------------------------------------------

// A LayoutManager which calculates preferred size based on AppListState and
// always lays out its children to the calculated preferred size.
class AssistantPageViewLayout : public views::LayoutManagerBase {
 public:
  explicit AssistantPageViewLayout(AssistantPageView* assistant_page_view)
      : assistant_page_view_(assistant_page_view) {}

  AssistantPageViewLayout(const AssistantPageViewLayout&) = delete;
  AssistantPageViewLayout& operator=(const AssistantPageViewLayout&) = delete;
  ~AssistantPageViewLayout() override = default;

  // views::LayoutManagerBase:
  gfx::Size GetPreferredSize(const views::View* host) const override {
    DCHECK_EQ(assistant_page_view_, host);
    return assistant_page_view_->contents_view()
        ->AdjustSearchBoxSizeToFitMargins(
            gfx::Size(kPreferredWidthDip,
                      GetPreferredHeightForWidth(host, kPreferredWidthDip)));
  }

  int GetPreferredHeightForWidth(const views::View* host,
                                 int width) const override {
    DCHECK_EQ(assistant_page_view_, host);

    // Calculate |preferred_height| for AppListState.
    int preferred_height = GetPreferredHeightForAppListState(
        assistant_page_view_->contents_view()->app_list_view());

    // Respect |host|'s minimum size.
    preferred_height =
        std::max(preferred_height, host->GetMinimumSize().height());

    // Snap to |kMaxHeightDip| if |child| exceeds |preferred_height|.
    for (const auto* child : host->children()) {
      if (child->GetHeightForWidth(width) > preferred_height)
        return kMaxHeightDip;
    }

    return preferred_height;
  }

  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override {
    // Always use preferred size for layout. Our |host| will be clipped to give
    // the appearance of animating its bounds during AppListState transitions
    // and this will ensure that our content remains in the desired location.
    const gfx::Size size = GetPreferredSize(host_view());
    const int left = (host_view()->width() - size.width()) / 2;
    const int top = 0;
    const gfx::Rect bounds = gfx::Rect(left, top, size.width(), size.height());

    views::ProposedLayout proposed_layout;
    proposed_layout.host_size = host_view()->size();
    for (auto* child : host_view()->children()) {
      proposed_layout.child_layouts.push_back(views::ChildLayout{
          child, child->GetVisible(), bounds, views::SizeBounds()});
    }

    return proposed_layout;
  }

 private:
  AssistantPageView* const assistant_page_view_;
};

}  // namespace

// AssistantPageView -----------------------------------------------------------

AssistantPageView::AssistantPageView(
    AssistantViewDelegate* assistant_view_delegate)
    : assistant_view_delegate_(assistant_view_delegate),
      min_height_dip_(kMinHeightDip) {
  InitLayout();

  if (AssistantController::Get())  // May be |nullptr| in tests.
    assistant_controller_observer_.Add(AssistantController::Get());

  if (AssistantUiController::Get())  // May be |nullptr| in tests.
    AssistantUiController::Get()->GetModel()->AddObserver(this);
}

AssistantPageView::~AssistantPageView() {
  if (AssistantUiController::Get())
    AssistantUiController::Get()->GetModel()->RemoveObserver(this);
}

const char* AssistantPageView::GetClassName() const {
  return "AssistantPageView";
}

gfx::Size AssistantPageView::GetMinimumSize() const {
  return gfx::Size(kPreferredWidthDip, min_height_dip_);
}

void AssistantPageView::OnBoundsChanged(const gfx::Rect& prev_bounds) {
  // The clip-rect set for page state animations needs to be reset when the
  // bounds change because page size change invalidates the previous bounds.
  // This allows content to properly follow target bounds w/ screen rotations.
  if (prev_bounds.size() != bounds().size())
    layer()->SetClipRect(gfx::Rect());

  if (!IsDrawn())
    return;

  // Until Assistant UI is closed, the view may grow in height but not shrink.
  min_height_dip_ = std::max(min_height_dip_, GetContentsBounds().height());
}

void AssistantPageView::RequestFocus() {
  if (!AssistantUiController::Get())  // May be |nullptr| in tests.
    return;

  switch (AssistantUiController::Get()->GetModel()->ui_mode()) {
    case AssistantUiMode::kLauncherEmbeddedUi:
      if (assistant_main_view_)
        assistant_main_view_->RequestFocus();
      break;
    case AssistantUiMode::kAmbientUi:
      NOTREACHED();
      break;
  }
}

void AssistantPageView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  View::GetAccessibleNodeData(node_data);
  node_data->SetName(l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_WINDOW));
}

void AssistantPageView::ChildPreferredSizeChanged(views::View* child) {
  MaybeUpdateAppListState(child->GetHeightForWidth(width()));
  PreferredSizeChanged();
}

void AssistantPageView::ChildVisibilityChanged(views::View* child) {
  if (!child->GetVisible())
    return;

  MaybeUpdateAppListState(child->GetHeightForWidth(width()));
}

void AssistantPageView::VisibilityChanged(views::View* starting_from,
                                          bool is_visible) {
  if (starting_from == this && !is_visible)
    min_height_dip_ = kMinHeightDip;
}

void AssistantPageView::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      // Prevents closing the AppListView when a click event is not handled.
      event->StopPropagation();
      break;
    default:
      break;
  }
}

void AssistantPageView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_DOUBLE_TAP:
    case ui::ET_GESTURE_LONG_PRESS:
    case ui::ET_GESTURE_LONG_TAP:
    case ui::ET_GESTURE_TWO_FINGER_TAP:
      // Prevents closing the AppListView when a tap event is not handled.
      event->StopPropagation();
      break;
    default:
      break;
  }
}

void AssistantPageView::OnWillBeShown() {
  // Our preferred size may require a change in AppListState in order to ensure
  // that the AssistantPageView renders fully on screen w/o being clipped. We do
  // this in OnWillBeShown(), as opposed to waiting for OnShown(), so that the
  // AppListState change animation can run in sync with page change animations.
  MaybeUpdateAppListState(GetPreferredSize().height());
}

void AssistantPageView::OnAnimationStarted(AppListState from_state,
                                           AppListState to_state) {
  // Animation is only needed when transitioning to/from Assistant.
  if (from_state != AppListState::kStateEmbeddedAssistant &&
      to_state != AppListState::kStateEmbeddedAssistant) {
    UpdatePageBoundsForState(to_state, contents_view()->GetContentsBounds(),
                             contents_view()->GetSearchBoxBounds(to_state));
    return;
  }

  const gfx::Rect contents_bounds = contents_view()->GetContentsBounds();

  const gfx::Rect from_rect =
      GetPageBoundsForState(from_state, contents_bounds,
                            contents_view()->GetSearchBoxBounds(from_state));

  const gfx::Rect to_rect = GetPageBoundsForState(
      to_state, contents_bounds, contents_view()->GetSearchBoxBounds(to_state));

  if (from_rect == to_rect)
    return;

  const int to_radius = contents_view()
                            ->GetSearchBoxView()
                            ->GetSearchBoxBorderCornerRadiusForState(to_state);

  // We are going to give the appearance of animating from |from_rect| to
  // |to_rect| using clip-rect animations. First, set bounds immediately to
  // target bounds...
  SetBoundsRect(to_rect);

  // ...but set the clip-rect to |from_rect| so that the user doesn't perceive
  // the change in bounds.
  gfx::Rect clip_rect = from_rect;
  clip_rect -= to_rect.OffsetFromOrigin();
  layer()->SetClipRect(clip_rect);

  // Animate the layer's clip-rect to the target bounds to give the appearance
  // of a bounds animation.
  {
    auto settings = contents_view()->CreateTransitionAnimationSettings(layer());

    ui::AnimationThroughputReporter reporter(
        settings->GetAnimator(),
        metrics_util::ForSmoothness(base::BindRepeating([](int value) {
          base::UmaHistogramPercentage(
              assistant::ui::kAssistantResizePageViewHistogram, value);
        })));

    layer()->SetClipRect(gfx::Rect(to_rect.size()));

    // Also animate corner radius for the view.
    // NOTE: This changes the shadow's corner radius immediately while |this|'s
    // corner radius changes gradually. This should be fine because this will be
    // unnoticeable to most users.
    view_shadow_->SetRoundedCornerRadius(to_radius);
  }

  // Animate the shadow's bounds through transform.
  {
    gfx::Transform transform;
    transform.Translate(from_rect.origin() - to_rect.origin());
    transform.Scale(static_cast<float>(from_rect.width()) / to_rect.width(),
                    static_cast<float>(from_rect.height()) / to_rect.height());
    view_shadow_->shadow()->layer()->SetTransform(transform);

    auto settings = contents_view()->CreateTransitionAnimationSettings(
        view_shadow_->shadow()->layer());
    view_shadow_->shadow()->layer()->SetTransform(gfx::Transform());
  }
}

gfx::Size AssistantPageView::GetPreferredSearchBoxSize() const {
  return gfx::Size(kPreferredWidthDip, kSearchBoxHeightDip);
}

base::Optional<int> AssistantPageView::GetSearchBoxTop(
    AppListViewState view_state) const {
  if (view_state == AppListViewState::kPeeking ||
      view_state == AppListViewState::kHalf) {
    return AppListConfig::instance().search_box_fullscreen_top_padding();
  }
  // For other view states, return base::nullopt so the ContentsView
  // sets the default search box widget origin.
  return base::nullopt;
}

views::View* AssistantPageView::GetFirstFocusableView() {
  return GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), /*reverse=*/false, /*dont_loop=*/false);
}

views::View* AssistantPageView::GetLastFocusableView() {
  return GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), /*reverse=*/true, /*dont_loop=*/false);
}

void AssistantPageView::AnimateYPosition(AppListViewState target_view_state,
                                         const TransformAnimator& animator,
                                         float default_offset) {
  // Assistant page view may host native views for its content. The native view
  // hosts use view to widget coordinate conversion to calculate the native view
  // bounds, and thus depend on the view transform values.
  // Make sure the view is laid out before starting the transform animation so
  // native views are not placed according to interim, animated page transform
  // value.
  layer()->GetAnimator()->StopAnimatingProperty(
      ui::LayerAnimationElement::TRANSFORM);
  if (needs_layout())
    Layout();

  animator.Run(default_offset, layer(), this);
  animator.Run(default_offset, view_shadow_->shadow()->shadow_layer(), nullptr);
}

void AssistantPageView::UpdatePageOpacityForState(AppListState state,
                                                  float search_box_opacity,
                                                  bool restore_opacity) {
  layer()->SetOpacity(search_box_opacity);
}

gfx::Rect AssistantPageView::GetPageBoundsForState(
    AppListState state,
    const gfx::Rect& contents_bounds,
    const gfx::Rect& search_box_bounds) const {
  // If transitioning to/from |kStateApps|, Assistant bounds will be animating
  // to/from |search_box_bounds|.
  if (state == AppListState::kStateApps)
    return search_box_bounds;

  // If transitioning to/from Assistant, Assistant bounds will be animating
  // to/from the bounds of the page associated with the specified |state|.
  if (state != AppListState::kStateEmbeddedAssistant) {
    return contents_view()
        ->GetPageView(contents_view()->GetPageIndexForState(state))
        ->GetPageBoundsForState(state, contents_bounds, search_box_bounds);
  }

  gfx::Rect bounds =
      gfx::Rect(gfx::Point(contents_bounds.x(), search_box_bounds.y()),
                GetPreferredSize());
  bounds.Offset((contents_bounds.width() - bounds.width()) / 2, 0);
  return bounds;
}

void AssistantPageView::OnAssistantControllerDestroying() {
  if (AssistantUiController::Get())  // May be |nullptr| in tests.
    AssistantUiController::Get()->GetModel()->RemoveObserver(this);

  if (AssistantController::Get())  // May be |nullptr| in tests.
    assistant_controller_observer_.Remove(AssistantController::Get());
}

void AssistantPageView::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  if (!assistant_view_delegate_)
    return;

  if (new_visibility != AssistantVisibility::kVisible) {
    min_height_dip_ = kMinHeightDip;
    return;
  }

  // Assistant page will get focus when widget shown.
  if (GetWidget() && GetWidget()->IsActive())
    RequestFocus();

  const bool prefer_voice =
      assistant_view_delegate_->IsTabletMode() ||
      AssistantState::Get()->launch_with_mic_open().value_or(false);
  if (!assistant::util::IsVoiceEntryPoint(entry_point.value(), prefer_voice)) {
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  }
}

void AssistantPageView::InitLayout() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  view_shadow_ = std::make_unique<ViewShadow>(this, kShadowElevation);
  view_shadow_->SetRoundedCornerRadius(
      kSearchBoxBorderCornerRadiusSearchResult);

  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  SetLayoutManager(std::make_unique<AssistantPageViewLayout>(this));

  // |assistant_view_delegate_| could be nullptr in test.
  if (!assistant_view_delegate_)
    return;

  assistant_main_view_ = AddChildView(
      std::make_unique<AssistantMainView>(assistant_view_delegate_));
}

void AssistantPageView::MaybeUpdateAppListState(int child_height) {
  auto* app_list_view = contents_view()->app_list_view();
  auto* widget = app_list_view->GetWidget();

  // |app_list_view| may not be initialized.
  if (!widget || !widget->IsVisible())
    return;

  // Update app list view state for |assistant_page_view_|.
  // Embedded Assistant Ui only has two sizes. The only state change is from
  // |kPeeking| to |kHalf| state.
  if (app_list_view->app_list_state() != AppListViewState::kPeeking)
    return;

  if (child_height > GetPreferredHeightForAppListState(app_list_view))
    app_list_view->SetState(AppListViewState::kHalf);
}

}  // namespace ash
