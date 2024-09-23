// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_page_view.h"

#include <algorithm>
#include <utility>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/assistant/assistant_main_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/search_box/search_box_constants.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/view_shadow.h"

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
    case AppListViewState::kFullscreenSearch:
      return kMaxHeightDip;
    default:
      return kMinHeightDip;
  }
}

bool IsInTabletMode() {
  // Shell might not has an instance in tests.
  return Shell::HasInstance() && Shell::Get()->IsInTabletMode();
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
    return GetPreferredSize(host, {});
  }

  gfx::Size GetPreferredSize(
      const views::View* host,
      const views::SizeBounds& available_size) const override {
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
    for (const views::View* child : host->children()) {
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
    for (views::View* child : host_view()->children()) {
      proposed_layout.child_layouts.push_back(views::ChildLayout{
          child, child->GetVisible(), bounds, views::SizeBounds()});
    }

    return proposed_layout;
  }

 private:
  const raw_ptr<AssistantPageView> assistant_page_view_;
};

}  // namespace

// AssistantPageView -----------------------------------------------------------

AssistantPageView::AssistantPageView(
    AssistantViewDelegate* assistant_view_delegate)
    : assistant_view_delegate_(assistant_view_delegate),
      min_height_dip_(kMinHeightDip) {
  InitLayout();

  if (AssistantController::Get())  // May be |nullptr| in tests.
    assistant_controller_observation_.Observe(AssistantController::Get());

  if (AssistantUiController::Get())  // May be |nullptr| in tests.
    AssistantUiController::Get()->GetModel()->AddObserver(this);

  display_observation_.Observe(display::Screen::GetScreen());

  GetViewAccessibility().SetRole(ax::mojom::Role::kPane);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_WINDOW));
}

AssistantPageView::~AssistantPageView() {
  if (AssistantUiController::Get())
    AssistantUiController::Get()->GetModel()->RemoveObserver(this);
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

  if (assistant_main_view_)
    assistant_main_view_->RequestFocus();
}

void AssistantPageView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantPageView::VisibilityChanged(views::View* starting_from,
                                          bool is_visible) {
  if (starting_from == this && !is_visible)
    min_height_dip_ = kMinHeightDip;
}

void AssistantPageView::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::EventType::kMousePressed:
      // Prevents closing the AppListView when a click event is not handled.
      event->StopPropagation();
      break;
    default:
      break;
  }
}

void AssistantPageView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::EventType::kGestureTap:
    case ui::EventType::kGestureDoubleTap:
    case ui::EventType::kGestureLongPress:
    case ui::EventType::kGestureLongTap:
    case ui::EventType::kGestureTwoFingerTap:
      // Prevents closing the AppListView when a tap event is not handled.
      event->StopPropagation();
      break;
    default:
      break;
  }
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
        metrics_util::ForSmoothnessV3(base::BindRepeating([](int value) {
          base::UmaHistogramPercentage(
              "Ash.Assistant.AnimationSmoothness.ResizeAssistantPageView",
              value);
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
    // `view_shadow_` can't be accurately scaled and translated because while
    // its bounds need animation, the shadow size needs to remain the same. This
    // causes the transformed shadow to be visually misplaced. To fix this,
    // inset the `from_rect` so that the transformed shadow is completely hidden
    // behind the view layer at the start of animation and slowly reveals itself
    // when animating to the proper size.
    gfx::Rect shadow_from_rect = from_rect;
    shadow_from_rect.Inset(kShadowElevation);

    const gfx::Transform transform = gfx::TransformBetweenRects(
        gfx::RectF(to_rect), gfx::RectF(shadow_from_rect));
    view_shadow_->shadow()->layer()->SetTransform(transform);

    auto settings = contents_view()->CreateTransitionAnimationSettings(
        view_shadow_->shadow()->layer());
    view_shadow_->shadow()->layer()->SetTransform(gfx::Transform());
  }
}

gfx::Size AssistantPageView::GetPreferredSearchBoxSize() const {
  return gfx::Size(kPreferredWidthDip, kSearchBoxHeightDip);
}

void AssistantPageView::UpdatePageOpacityForState(AppListState state,
                                                  float search_box_opacity) {
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

  if (AssistantController::Get()) {
    // May be |nullptr| in tests.
    DCHECK(assistant_controller_observation_.IsObservingSource(
        AssistantController::Get()));
    assistant_controller_observation_.Reset();
  }
}

void AssistantPageView::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    std::optional<AssistantEntryPoint> entry_point,
    std::optional<AssistantExitPoint> exit_point) {
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

void AssistantPageView::OnDisplayTabletStateChanged(
    display::TabletState state) {
  switch (state) {
    case display::TabletState::kEnteringTabletMode:
    case display::TabletState::kExitingTabletMode:
      // Do nothing when the tablet mode is in process of changing.
      break;
    case display::TabletState::kInTabletMode:
      UpdateBackground(/*in_tablet_mode=*/true);
      break;
    case display::TabletState::kInClamshellMode:
      UpdateBackground(/*in_tablet_mode=*/false);
  }
}

void AssistantPageView::OnThemeChanged() {
  views::View::OnThemeChanged();

  UpdateBackground(IsInTabletMode());
}

void AssistantPageView::InitLayout() {
  // Use a solid color layer. The color is set in OnThemeChanged().
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  layer()->SetFillsBoundsOpaquely(false);

  view_shadow_ = std::make_unique<views::ViewShadow>(this, kShadowElevation);
  view_shadow_->SetRoundedCornerRadius(
      kSearchBoxBorderCornerRadiusSearchResult);

  SetLayoutManager(std::make_unique<AssistantPageViewLayout>(this));

  // |assistant_view_delegate_| could be nullptr in test.
  if (!assistant_view_delegate_)
    return;

  assistant_main_view_ = AddChildView(
      std::make_unique<AssistantMainView>(assistant_view_delegate_));
}

void AssistantPageView::UpdateBackground(bool in_tablet_mode) {
  // Blur
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  // Color
  const auto* color_provider =
      GetWidget() ? GetWidget()->GetColorProvider() : nullptr;

  // ColorProvide might be nullptr in tests or this function is triggered before
  // `this` is added to the view hierarchy.
  if (color_provider)
    layer()->SetColor(color_provider->GetColor(kColorAshShieldAndBase80));
  else
    layer()->SetColor(SK_ColorWHITE);
}

BEGIN_METADATA(AssistantPageView)
END_METADATA

}  // namespace ash
