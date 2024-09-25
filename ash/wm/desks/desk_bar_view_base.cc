// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_bar_view_base.h"

#include <algorithm>
#include <vector>

#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_action_button.h"
#include "ash/wm/desks/desk_action_view.h"
#include "ash/wm/desks/desk_mini_view_animations.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desk_profiles_button.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "ash/wm/desks/templates/saved_desk_presenter.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_metrics.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/work_area_insets.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/utils/haptics_util.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/background.h"
#include "ui/views/event_monitor.h"
#include "ui/views/highlight_border.h"
#include "ui/views/view_class_properties.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

// Duration of delay when Bento Bar Desk Button is clicked.
constexpr base::TimeDelta kAnimationDelayDuration = base::Milliseconds(100);

// Check whether there are any external keyboards.
bool HasExternalKeyboard() {
  for (const ui::InputDevice& device :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (device.type != ui::InputDeviceType::INPUT_DEVICE_INTERNAL) {
      return true;
    }
  }
  return false;
}

// Initialize a scoped layer animation settings for scroll view contents.
void InitScrollContentsAnimationSettings(
    ui::ScopedLayerAnimationSettings& settings) {
  settings.SetTransitionDuration(kDeskBarScrollDuration);
  settings.SetTweenType(gfx::Tween::ACCEL_20_DECEL_60);
}

gfx::Rect GetGestureEventScreenRect(const ui::Event& event) {
  CHECK(event.IsGestureEvent());
  return event.AsGestureEvent()->details().bounding_box();
}

// Sets up background for the desk bar. There could be 3 cases:
//   1) desk button bar
//      A separate view will be used as background for animation purpose.
//   2) overview bar with forest
//      No background.
//   3) overview bar without forest
//      The bar itself serves as the background.
void MaybeSetupBackgroundView(DeskBarViewBase* bar_view) {
  const bool type_is_desk_button =
      bar_view->type() == DeskBarViewBase::Type::kDeskButton;

  auto* view = type_is_desk_button ? bar_view->background_view() : bar_view;
  view->SetPaintToLayer();

  auto* layer = view->layer();
  layer->SetFillsBoundsOpaquely(false);

  if (features::IsForestFeatureEnabled() && !type_is_desk_button) {
    // Forest feature needs a transparent desks bar background. Still needs the
    // view layer to perform animations.
    return;
  }

  if (features::IsBackgroundBlurEnabled()) {
    layer->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  }

  const int corner_radius = type_is_desk_button
                                ? kDeskBarCornerRadiusOverviewDeskButton
                                : kDeskBarCornerRadiusOverview;
  view->SetBorder(std::make_unique<views::HighlightBorder>(
      corner_radius, views::HighlightBorder::Type::kHighlightBorderNoShadow));
  layer->SetRoundedCornerRadius(gfx::RoundedCornersF(corner_radius));
  view->SetBackground(
      views::CreateThemedSolidBackground(kColorAshShieldAndBase80));
}

}  // namespace

// -----------------------------------------------------------------------------
// DeskBarScrollViewLayout:

// All the desk bar contents except the background view are added to
// be the children of the `contents_view_`, which is a child of `scroll_view_`
// if scrolling is required. `DeskBarScrollViewLayout` will help lay out the
// contents.
class DeskBarScrollViewLayout : public views::LayoutManager {
 public:
  explicit DeskBarScrollViewLayout(DeskBarViewBase* bar_view)
      : bar_view_(bar_view) {}
  DeskBarScrollViewLayout(const DeskBarScrollViewLayout&) = delete;
  DeskBarScrollViewLayout& operator=(const DeskBarScrollViewLayout&) = delete;
  ~DeskBarScrollViewLayout() override = default;

  int GetContentViewX(int contents_width) const {
    // The x of the first mini view should include the focus ring thickness and
    // padding into consideration, otherwise the focus ring won't be drawn on
    // the left side of the first mini view. `bar_view` is centralized in
    // overview mode or when shelf is on the bottom. When shelf is on the
    // left/right, bar view anchors to the desk button.
    const auto shelf_type = Shelf::ForWindow(bar_view_->root())->alignment();
    if (bar_view_->type() == DeskBarViewBase::Type::kOverview ||
        shelf_type == ShelfAlignment::kBottom ||
        shelf_type == ShelfAlignment::kBottomLocked) {
      return (width_ - contents_width) / 2 +
             kDeskBarDeskPreviewViewFocusRingThicknessAndPadding;
    }

    if (shelf_type == ShelfAlignment::kLeft) {
      return kDeskBarDeskPreviewViewFocusRingThicknessAndPadding;
    }

    CHECK_EQ(shelf_type, ShelfAlignment::kRight);
    return width_ - contents_width +
           kDeskBarDeskPreviewViewFocusRingThicknessAndPadding;
  }

  void LayoutBackground() {
    if (!bar_view_->background_view_) {
      return;
    }

    const ShelfAlignment shelf_alignment =
        Shelf::ForWindow(bar_view_->root_)->alignment();
    const gfx::Rect preferred_bounds =
        gfx::Rect(bar_view_->CalculatePreferredSize({}));
    const gfx::Rect current_bounds = gfx::Rect(bar_view_->size());
    gfx::Rect new_bounds = preferred_bounds;
    if (shelf_alignment == ShelfAlignment::kBottom) {
      new_bounds = current_bounds;
      new_bounds.ClampToCenteredSize(preferred_bounds.size());
    } else if ((shelf_alignment == ShelfAlignment::kLeft) ==
               base::i18n::IsRTL()) {
      new_bounds.Offset(current_bounds.width() - preferred_bounds.width(), 0);
    }
    bar_view_->background_view_->SetBoundsRect(new_bounds);
  }

  // Layout the label which is shown below the desk icon button when the button
  // is at active state.
  void LayoutDeskIconButtonLabel(views::Label* label,
                                 const gfx::Rect& icon_button_bounds,
                                 DeskNameView* desk_name_view,
                                 int label_text_id) {
    label->SetText(gfx::ElideText(
        l10n_util::GetStringUTF16(label_text_id), gfx::FontList(),
        icon_button_bounds.width() - desk_name_view->GetInsets().width(),
        gfx::ELIDE_TAIL));

    const gfx::Size button_label_size =
        label->GetPreferredSize(views::SizeBounds(label->width(), {}));

    label->SetBoundsRect(gfx::Rect(
        gfx::Point(
            icon_button_bounds.x() +
                ((icon_button_bounds.width() - button_label_size.width()) / 2),
            icon_button_bounds.bottom() +
                kDeskBarDeskIconButtonAndLabelSpacing),
        gfx::Size(button_label_size.width(), desk_name_view->height())));
  }

  // Updates the visibility of child views based on current `bar_view_`.
  void UpdateChildViewsVisibility() {
    auto* default_desk_button = bar_view_->default_desk_button();
    auto* new_desk_button = bar_view_->new_desk_button();
    auto* library_button = bar_view_->library_button();
    auto* library_button_label = bar_view_->library_button_label();
    const bool zero_state = bar_view_->IsZeroState();
    default_desk_button->SetVisible(zero_state);
    new_desk_button->SetVisible(true);
    bar_view_->UpdateNewDeskButtonLabelVisibility(
        !zero_state &&
            new_desk_button->state() == DeskIconButton::State::kActive,
        // Already in an active layout. No need to trigger another one.
        /*layout_if_changed=*/false);
    if (library_button) {
      library_button->SetVisible(bar_view_->ShouldShowLibraryUi());
    }
    if (library_button_label) {
      library_button_label->SetVisible(!zero_state &&
                                       library_button->state() ==
                                           DeskIconButton::State::kActive);
    }
  }

  // views::LayoutManager:
  void Layout(views::View* host) override {
    TRACE_EVENT0("ui", "DeskBarScrollViewLayout::Layout");

    const gfx::Rect scroll_bounds =
        bar_view_->GetTopLevelViewWithContents().bounds();

    // Update visibility of child views so that `GetPreferredSize()` returns
    // correct size.
    UpdateChildViewsVisibility();

    const gfx::Size contents_size = host->GetPreferredSize();

    // `host` here is `contents_view_`.
    if (bar_view_->IsZeroState()) {
      host->SetBoundsRect(scroll_bounds);

      // Update default desk button. In addition, update its button text since
      // it may change while removing a desk and going back to the zero state.
      // Make sure default desk button is always visible while in zero state
      // bar.
      auto* default_desk_button = bar_view_->default_desk_button();
      default_desk_button->SetBoundsRect(gfx::Rect(
          gfx::Point((scroll_bounds.width() - contents_size.width()) / 2,
                     kDeskBarZeroStateY),
          default_desk_button->GetPreferredSize()));
      default_desk_button->UpdateLabelText();

      // Update new desk button.
      auto* new_desk_button = bar_view_->new_desk_button();
      new_desk_button->SetBoundsRect(
          gfx::Rect(gfx::Point(default_desk_button->bounds().right() +
                                   kDeskBarZeroStateButtonSpacing,
                               kDeskBarZeroStateY),
                    new_desk_button->GetPreferredSize()));

      // Update library button.
      if (auto* library_button = bar_view_->library_button()) {
        library_button->SetBoundsRect(
            gfx::Rect(gfx::Point(new_desk_button->bounds().right() +
                                     kDeskBarZeroStateButtonSpacing,
                                 kDeskBarZeroStateY),
                      library_button->GetPreferredSize()));
      }

      return;
    }

    std::vector<raw_ptr<DeskMiniView, VectorExperimental>> mini_views =
        bar_view_->mini_views();
    if (mini_views.empty()) {
      return;
    }

    // When RTL is enabled, we still want desks to be laid our in LTR, to match
    // the spatial order of desks. Therefore, we reverse the order of the mini
    // views before laying them out.
    const bool is_rtl = base::i18n::IsRTL();
    if (is_rtl) {
      base::ranges::reverse(mini_views);
    }

    width_ = std::max(scroll_bounds.width(), contents_size.width());

    // Update the size of the `host`, which is `contents_view_` here.
    // This is done to make sure its size can be updated on mini views' adding
    // or removing, then `scroll_view_` will know whether the contents need to
    // be scrolled or not.
    host->SetSize(gfx::Size(width_, contents_size.height()));

    const int increment = is_rtl ? -1 : 1;
    const int y =
        kDeskBarMiniViewsY - mini_views[0]->GetPreviewBorderInsets().top();
    const gfx::Size mini_view_size = mini_views[0]->GetPreferredSize();
    auto layout_mini_views = [&](int& x) {
      const int start = is_rtl ? mini_views.size() - 1 : 0;
      const int end = is_rtl ? -1 : mini_views.size();
      const int delta_x =
          (mini_view_size.width() + kDeskBarMiniViewsSpacing) * increment;
      for (int i = start; i != end; i += increment) {
        auto* mini_view = mini_views[i].get();
        mini_view->SetBoundsRect(
            gfx::Rect(gfx::Point(is_rtl ? x - mini_view_size.width() : x, y),
                      mini_view_size));
        x += delta_x;
      }
    };
    auto* desk_name_view = mini_views[0]->desk_name_view();
    auto layout_new_desk_button = [&](int& x) {
      auto* new_desk_button = bar_view_->new_desk_button();
      const gfx::Size new_desk_button_size =
          new_desk_button->GetPreferredSize();
      const gfx::Rect new_desk_button_bounds(gfx::Rect(
          gfx::Point(is_rtl ? x - new_desk_button_size.width() : x, y),
          new_desk_button_size));
      new_desk_button->SetBoundsRect(new_desk_button_bounds);
      if (bar_view_->new_desk_button_label()) {
        LayoutDeskIconButtonLabel(bar_view_->new_desk_button_label(),
                                  new_desk_button_bounds, desk_name_view,
                                  IDS_ASH_DESKS_NEW_DESK_BUTTON_LABEL);
      }
      x +=
          (new_desk_button_size.width() + kDeskBarMiniViewsSpacing) * increment;
    };
    auto layout_library_button = [&](int& x) {
      auto* library_button = bar_view_->library_button();
      if (!library_button) {
        return;
      }
      const gfx::Size library_button_size =
          library_button ? library_button->GetPreferredSize() : gfx::Size();
      const gfx::Rect library_button_bounds(
          gfx::Rect(gfx::Point(is_rtl ? x - library_button_size.width() : x, y),
                    library_button_size));
      library_button->SetBoundsRect(library_button_bounds);
      LayoutDeskIconButtonLabel(
          bar_view_->library_button_label(), library_button_bounds,
          desk_name_view,
          /*label_text_id=*/
          saved_desk_util::AreDesksTemplatesEnabled()
              ? IDS_ASH_DESKS_TEMPLATES_DESKS_BAR_BUTTON_LIBRARY
              : IDS_ASH_DESKS_TEMPLATES_DESKS_BAR_BUTTON_SAVED_FOR_LATER);
      x += (library_button_size.width() + kDeskBarMiniViewsSpacing) * increment;
    };

    // When the desk bar is in middle of bar shrink animation, the bounds of
    // scroll view contents is actually wider than `contents_width`. When RTL is
    // not on, we layout UIs from left to right with `x` indicating the current
    // available position to place the next UI; to make animation work for RTL,
    // we need to layout from right to left.
    // TODO(b/301665941): improve layout calculation for RTL.
    if (is_rtl) {
      int x = width_ - GetContentViewX(contents_size.width());
      layout_library_button(x);
      layout_new_desk_button(x);
      layout_mini_views(x);
    } else {
      int x = GetContentViewX(contents_size.width());
      layout_mini_views(x);
      layout_new_desk_button(x);
      layout_library_button(x);
    }

    LayoutBackground();
  }

  // views::LayoutManager:
  gfx::Size GetPreferredSize(const views::View* host) const override {
    return GetPreferredSize(host, {});
  }

  gfx::Size GetPreferredSize(
      const views::View* host,
      const views::SizeBounds& available_size) const override {
    int width = 0;
    std::vector<views::View*> child_views;

    for (ash::DeskMiniView* mini_view : bar_view_->mini_views_) {
      child_views.emplace_back(mini_view);
    }

    child_views.emplace_back(bar_view_->default_desk_button_);
    child_views.emplace_back(bar_view_->new_desk_button_);
    child_views.emplace_back(bar_view_->library_button_);

    const int child_spacing =
        bar_view_->state_ == DeskBarViewBase::State::kExpanded
            ? kDeskBarMiniViewsSpacing
            : kDeskBarZeroStateButtonSpacing;
    for (auto* child : child_views) {
      if (!child || !child->GetVisible()) {
        continue;
      }
      if (width) {
        width += child_spacing;
      }
      width += child->GetPreferredSize().width();
    }
    width += kDeskBarDeskPreviewViewFocusRingThicknessAndPadding * 2;

    return gfx::Size(
        width, DeskBarViewBase::GetPreferredBarHeight(
                   bar_view_->root(), bar_view_->type_, bar_view_->state_));
  }

 private:
  raw_ptr<DeskBarViewBase> bar_view_;

  // Width of the scroll view. It is the contents' preferred width if it exceeds
  // the desk bar view's width or just the desk bar view's width if not.
  int width_ = 0;
};

// -----------------------------------------------------------------------------
// DeskBarHoverObserver:

class DeskBarHoverObserver : public ui::EventObserver {
 public:
  DeskBarHoverObserver(DeskBarViewBase* owner, aura::Window* widget_window)
      : owner_(owner),
        event_monitor_(views::EventMonitor::CreateWindowMonitor(
            this,
            widget_window,
            {ui::EventType::kMousePressed, ui::EventType::kMouseDragged,
             ui::EventType::kMouseReleased, ui::EventType::kMouseMoved,
             ui::EventType::kMouseEntered, ui::EventType::kMouseExited,
             ui::EventType::kGestureLongPress, ui::EventType::kGestureLongTap,
             ui::EventType::kGestureTap, ui::EventType::kGestureTapDown})) {}

  DeskBarHoverObserver(const DeskBarHoverObserver&) = delete;
  DeskBarHoverObserver& operator=(const DeskBarHoverObserver&) = delete;

  ~DeskBarHoverObserver() override = default;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    switch (event.type()) {
      case ui::EventType::kMousePressed:
      case ui::EventType::kMouseDragged:
      case ui::EventType::kMouseReleased:
      case ui::EventType::kMouseMoved:
      case ui::EventType::kMouseEntered:
      case ui::EventType::kMouseExited:
        owner_->OnHoverStateMayHaveChanged();
        break;

      case ui::EventType::kGestureLongPress:
      case ui::EventType::kGestureLongTap:
        owner_->OnGestureTap(GetGestureEventScreenRect(event),
                             /*is_long_gesture=*/true);
        break;

      case ui::EventType::kGestureTap:
      case ui::EventType::kGestureTapDown:
        owner_->OnGestureTap(GetGestureEventScreenRect(event),
                             /*is_long_gesture=*/false);
        break;

      default:
        NOTREACHED();
    }
  }

 private:
  raw_ptr<DeskBarViewBase> owner_;

  std::unique_ptr<views::EventMonitor> event_monitor_;
};

// -----------------------------------------------------------------------------
// DeskBarViewBase::PostLayoutOperation:

// Addresses this common sequence in `DeskBarViewBase`:
// 1) Some desk ui-related event happens (ex: a new desk is added).
// 2) The desk bar layout must happen as a result. Layout is asynchronous.
// 3) After the layout, some remaining operations (often an animation) must
//    be performed. But these operations depends on the layout completing first.
//
// `PostLayoutOperation` represents the content at step 3. It is a short-lived
// class that created and cached in step 1, and then run after the next layout
// completes.
class DeskBarViewBase::PostLayoutOperation {
 public:
  virtual ~PostLayoutOperation() = default;

  // Optional: Allows the implementation to capture any required UI state
  // immediately before the layout happens and said state is changed. Always
  // called right before the layout.
  virtual void InitializePreLayout() {}

  // Called after the layout completes. Runs the content in step 3. The
  // `PostLayoutOperation` is destroyed immediately after `Run()`.
  virtual void Run() = 0;

 protected:
  explicit PostLayoutOperation(DeskBarViewBase* bar_view)
      : bar_view_(bar_view) {
    CHECK(bar_view_);
  }

  const raw_ptr<DeskBarViewBase> bar_view_;
};

// Runs animations when a new desk is added.
class DeskBarViewBase::AddDeskAnimation
    : public DeskBarViewBase::PostLayoutOperation {
 public:
  AddDeskAnimation(DeskBarViewBase* bar_view,
                   const gfx::Rect& old_bar_bounds,
                   std::vector<DeskMiniView*> new_mini_views)
      : PostLayoutOperation(bar_view),
        old_bar_bounds_(old_bar_bounds),
        new_mini_views_(std::move(new_mini_views)) {}

  // DeskBarViewBase::PostLayoutOperation:
  void InitializePreLayout() override {
    views_previous_x_map_ = bar_view_->GetAnimatableViewsCurrentXMap();
  }

  void Run() override {
    // Filter out mini views that were erased between the time it was created
    // and the layout occurred. In practice, this should be extremely rare or
    // even non-existent but is theoretically possible due to the asynchronous
    // nature the layout operation.
    auto new_mini_view_it = new_mini_views_.begin();
    while (new_mini_view_it != new_mini_views_.end()) {
      if (base::Contains(bar_view_->mini_views_, *new_mini_view_it)) {
        ++new_mini_view_it;
      } else {
        new_mini_view_it = new_mini_views_.erase(new_mini_view_it);
      }
    }

    if (bar_view_->type_ == Type::kDeskButton) {
      PerformDeskBarAddDeskAnimation(bar_view_, old_bar_bounds_);
    }
    PerformAddDeskMiniViewAnimation(new_mini_views_);
    PerformDeskBarChildViewShiftAnimation(bar_view_, views_previous_x_map_);
  }

 private:
  const gfx::Rect old_bar_bounds_;
  std::vector<DeskMiniView*> new_mini_views_;
  base::flat_map<views::View*, int> views_previous_x_map_;
};

// Scales the size of the `new_desk_button_` or `library_button_`.
class DeskBarViewBase::DeskIconButtonScaleAnimation
    : public DeskBarViewBase::PostLayoutOperation {
 public:
  DeskIconButtonScaleAnimation(DeskBarViewBase* bar_view,
                               DeskIconButton* button)
      : PostLayoutOperation(bar_view), button_(button) {
    CHECK(button_);
  }

  // DeskBarViewBase::PostLayoutOperation:
  void InitializePreLayout() override {
    begin_x_ = bar_view_->GetFirstMiniViewXOffset();
    old_bounds_ = button_->GetBoundsInScreen();
  }

  void Run() override {
    const gfx::RectF new_bounds = gfx::RectF(button_->GetBoundsInScreen());
    gfx::Transform scale_transform;
    const int shift_x = begin_x_ - bar_view_->GetFirstMiniViewXOffset();
    scale_transform.Translate(shift_x, 0);
    if (!old_bounds_.IsEmpty()) {
      CHECK(!new_bounds.IsEmpty());
      scale_transform.Scale(old_bounds_.width() / new_bounds.width(),
                            old_bounds_.height() / new_bounds.height());
    }

    PerformDeskIconButtonScaleAnimation(button_, bar_view_, scale_transform,
                                        shift_x);

    bar_view_->MaybeRefreshOverviewGridBounds();
  }

 private:
  const raw_ptr<DeskIconButton> button_;
  int begin_x_ = 0;
  gfx::Rect old_bounds_;
};

// Runs animations when the library button visibility changes.
class DeskBarViewBase::LibraryButtonVisibilityAnimation
    : public DeskBarViewBase::PostLayoutOperation {
 public:
  explicit LibraryButtonVisibilityAnimation(DeskBarViewBase* bar_view)
      : PostLayoutOperation(bar_view) {}

  // DeskBarViewBase::PostLayoutOperation:
  void InitializePreLayout() override {
    begin_x_ = bar_view_->GetFirstMiniViewXOffset();
  }

  void Run() override {
    // This call shifts the transforms of the mini views and new desk button and
    // then animates to the identity transform.
    PerformLibraryButtonVisibilityAnimation(
        bar_view_->mini_views_, bar_view_->new_desk_button_,
        begin_x_ - bar_view_->GetFirstMiniViewXOffset());
  }

 private:
  int begin_x_ = 0;
};

// Scrolls to make the new desk mini view visible in the desk bar when a new
// desk is created.
class DeskBarViewBase::NewDeskButtonPressedScroll
    : public DeskBarViewBase::PostLayoutOperation {
 public:
  explicit NewDeskButtonPressedScroll(DeskBarViewBase* bar_view)
      : PostLayoutOperation(bar_view) {}

  // DeskBarViewBase::PostLayoutOperation:
  void Run() override {
    bar_view_->NudgeDeskName(bar_view_->mini_views_.size() - 1);

    // TODO(b/277081702): When desk order is adjusted for RTL, remove the check
    // below to always make new desk button visible.
    if (!base::i18n::IsRTL()) {
      bar_view_->ScrollToShowViewIfNecessary(bar_view_->new_desk_button_);
    }
  }
};

// Runs animations when a desk is removed.
class DeskBarViewBase::RemoveDeskAnimation
    : public DeskBarViewBase::PostLayoutOperation {
 public:
  RemoveDeskAnimation(DeskBarViewBase* bar_view,
                      DeskMiniView* removed_mini_view)
      : PostLayoutOperation(bar_view),
        removed_mini_view_(bar_view_->type_ == DeskBarViewBase::Type::kOverview
                               ? removed_mini_view
                               : nullptr) {}

  // DeskBarViewBase::PostLayoutOperation:
  void InitializePreLayout() override {
    if (bar_view_->type_ == DeskBarViewBase::Type::kDeskButton) {
      old_background_bounds_ = bar_view_->background_view_->GetBoundsInScreen();
    }
    views_previous_x_map_ = bar_view_->GetAnimatableViewsCurrentXMap();
  }

  void Run() override {
    if (removed_mini_view_) {
      DeskMiniView* removed_mini_view = removed_mini_view_;
      // The mini view is deleted in the call below. Set to null to avoid a
      // dangling `raw_ptr` reference.
      removed_mini_view_ = nullptr;
      PerformRemoveDeskMiniViewAnimation(removed_mini_view);
    } else {
      PerformDeskBarRemoveDeskAnimation(bar_view_, old_background_bounds_);
    }
    PerformDeskBarChildViewShiftAnimation(bar_view_, views_previous_x_map_);
    bar_view_->MaybeUpdateDeskActionButtonTooltips();
  }

 private:
  raw_ptr<DeskMiniView> removed_mini_view_;
  gfx::Rect old_background_bounds_;
  base::flat_map<views::View*, int> views_previous_x_map_;
};

// Runs animations when a desk is reordered.
class DeskBarViewBase::ReorderDeskAnimation
    : public DeskBarViewBase::PostLayoutOperation {
 public:
  ReorderDeskAnimation(DeskBarViewBase* bar_view,
                       size_t old_index,
                       size_t new_index)
      : PostLayoutOperation(bar_view),
        old_index_(old_index),
        new_index_(new_index) {}

  // DeskBarViewBase::PostLayoutOperation:
  void Run() override {
    const auto& mini_views = bar_view_->mini_views_;
    // Don't crash if the mini view was erased between the time it was reordered
    // and the layout occurred. In practice, this should be extremely rare or
    // even non-existent but is theoretically possible due to the asynchronous
    // nature the layout operation.
    if (old_index_ >= mini_views.size() || new_index_ >= mini_views.size()) {
      return;
    }
    PerformReorderDeskMiniViewAnimation(old_index_, new_index_, mini_views);
    bar_view_->MaybeUpdateDeskActionButtonTooltips();
  }

 private:
  const size_t old_index_;
  const size_t new_index_;
};

// Scrolls to make the active desk visible in the desk bar when the desk bar is
// opened.
class DeskBarViewBase::ScrollForActiveMiniView
    : public DeskBarViewBase::PostLayoutOperation {
 public:
  explicit ScrollForActiveMiniView(DeskBarViewBase* bar_view)
      : PostLayoutOperation(bar_view) {}

  // DeskBarViewBase::PostLayoutOperation:
  void Run() override {
    // When the bar is initialized, scroll to make active desk mini view
    // visible.
    auto it = base::ranges::find_if(
        bar_view_->mini_views_,
        [](DeskMiniView* mini_view) { return mini_view->desk()->is_active(); });
    if (it != bar_view_->mini_views_.end()) {
      bar_view_->ScrollToShowViewIfNecessary(*it);
    }
  }
};

DeskBarViewBase::DeskBarViewBase(
    aura::Window* root,
    Type type,
    base::WeakPtr<WindowOcclusionCalculator> window_occlusion_calculator)
    : type_(type),
      state_(GetPreferredState(type)),
      root_(root),
      window_occlusion_calculator_(window_occlusion_calculator) {
  CHECK(root && root->IsRootWindow());

  // Background layer is needed for desk bar animation.
  if (type_ == Type::kDeskButton) {
    background_view_ = AddChildView(std::make_unique<views::View>());
  }

  MaybeSetupBackgroundView(this);

  if (chromeos::features::AreOverviewSessionInitOptimizationsEnabled()) {
    contents_view_ = AddChildView(std::make_unique<views::View>());
  } else {
    InitScrolling();
  }

  default_desk_button_ =
      contents_view_->AddChildView(std::make_unique<DefaultDeskButton>(this));
  new_desk_button_ =
      contents_view_->AddChildView(std::make_unique<DeskIconButton>(
          this, &kDesksNewDeskButtonIcon,
          l10n_util::GetStringUTF16(IDS_ASH_DESKS_NEW_DESK_BUTTON),
          cros_tokens::kCrosSysOnPrimary, cros_tokens::kCrosSysPrimary,
          /*initially_enabled=*/DesksController::Get()->CanCreateDesks(),
          base::BindRepeating(
              &DeskBarViewBase::OnNewDeskButtonPressed, base::Unretained(this),
              type_ == Type::kDeskButton
                  ? DesksCreationRemovalSource::kDeskButtonDeskBarButton
                  : DesksCreationRemovalSource::kButton),
          base::BindRepeating(&DeskBarViewBase::InitScrollingIfRequired,
                              base::Unretained(this))));
  new_desk_button_->SetProperty(views::kElementIdentifierKey,
                                kOverviewDeskBarNewDeskButtonElementId);

  contents_view_->SetLayoutManager(
      std::make_unique<DeskBarScrollViewLayout>(this));

  DesksController::Get()->AddObserver(this);
}

DeskBarViewBase::~DeskBarViewBase() {
  TRACE_EVENT0("ui", "DeskBarViewBase::~DeskBarViewBase");

  DesksController::Get()->RemoveObserver(this);
  if (drag_view_) {
    EndDragDesk(drag_view_, /*end_by_user=*/false);
  }
}

// static
int DeskBarViewBase::GetPreferredBarHeight(aura::Window* root,
                                           Type type,
                                           State state) {
  int height = 0;
  switch (type) {
    case Type::kDeskButton:
      CHECK_EQ(State::kExpanded, state);
      height =
          DeskPreviewView::GetHeight(root) + kDeskBarNonPreviewAllocatedHeight;
      break;
    case Type::kOverview:
      if (state == State::kZero) {
        height = kDeskBarZeroStateHeight;
      } else {
        height = DeskPreviewView::GetHeight(root) +
                 (features::IsForestFeatureEnabled()
                      ? kExpandedDeskBarHeight
                      : kDeskBarNonPreviewAllocatedHeight);
      }
      break;
  }

  return height;
}

// static
DeskBarViewBase::State DeskBarViewBase::GetPreferredState(Type type) {
  State state = State::kZero;
  switch (type) {
    case Type::kDeskButton:
      // Desk button desk bar is always expanded.
      state = State::kExpanded;
      break;
    case Type::kOverview: {
      // Overview desk bar can be zero state if both conditions below are true.
      //   - there is only one desk;
      //   - not currently showing saved desk library;
      OverviewController* overview_controller =
          Shell::Get()->overview_controller();
      DesksController* desk_controller = DesksController::Get();
      if (desk_controller->GetNumberOfDesks() == 1 &&
          overview_controller->InOverviewSession() &&
          !overview_controller->overview_session()
               ->IsShowingSavedDeskLibrary()) {
        state = State::kZero;
      } else {
        state = State::kExpanded;
      }
      break;
    }
  }

  return state;
}

// static
std::unique_ptr<views::Widget> DeskBarViewBase::CreateDeskWidget(
    aura::Window* root,
    const gfx::Rect& bounds,
    Type type) {
  CHECK(root && root->IsRootWindow());

  std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.accept_events = true;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.bounds = bounds;

  // The contents of this widget will have a textured layer, so we can mark
  // the widget's layer as not drawn.
  params.layer_type = ui::LAYER_NOT_DRAWN;

  if (type == Type::kOverview) {
    // Overview desk bar should live under the currently-active desk container
    // on `root`.
    params.context = root;
    params.name = "OverviewDeskBarWidget";
    // Even though this widget exists on the active desk container, it should
    // not show up in the MRU list, and it should not be mirrored in the desks
    // mini_views.
    params.init_properties_container.SetProperty(kOverviewUiKey, true);
    params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  } else {
    // Desk button desk bar should live under the shelf bubble container on
    // `root`.
    params.parent =
        Shell::GetContainer(root, kShellWindowId_ShelfBubbleContainer);
    params.name = "DeskButtonDeskBarWidget";
  }

  widget->Init(std::move(params));
  wm::SetWindowVisibilityAnimationTransition(widget->GetNativeWindow(),
                                             wm::ANIMATE_NONE);
  return widget;
}

void DeskBarViewBase::Layout(PassKey) {
  TRACE_EVENT0("ui", "DeskBarViewBase::Layout");

  if (pause_layout_) {
    return;
  }

  // It's possible that this is not owned by the overview grid anymore, because
  // when exiting overview, the bar stays alive for animation.
  if (type_ == Type::kOverview && !overview_grid_) {
    return;
  }

  // Move to a local variable on the stack in case `PostLayoutOperation::Run()`
  // synchronously calls `DeskBarViewBase::Layout()` again.
  auto post_layout_operations = std::move(pending_post_layout_operations_);
  pending_post_layout_operations_.clear();
  for (const auto& post_layout_operation : post_layout_operations) {
    post_layout_operation->InitializePreLayout();
  }

  // Scroll buttons are kept `scroll_view_padding` away from the edge of the
  // scroll view. So the horizontal padding of the scroll view is set to
  // guarantee enough space for the scroll buttons.
  const gfx::Insets insets = (type_ == Type::kOverview)
                                 ? overview_grid_->GetGridInsets()
                                 : gfx::Insets();
  CHECK(insets.left() == insets.right());
  const int scroll_view_padding =
      (type_ == Type::kOverview
           ? kDeskBarScrollViewMinimumHorizontalPaddingOverview
           : kDeskBarScrollViewMinimumHorizontalPaddingDeskButton);
  const int horizontal_padding = std::max(scroll_view_padding, insets.left());
  if (IsScrollingInitialized()) {
    left_scroll_button_->SetBounds(horizontal_padding - scroll_view_padding,
                                   bounds().y(), kDeskBarScrollButtonWidth,
                                   bounds().height());
    right_scroll_button_->SetBounds(
        bounds().right() - horizontal_padding -
            (kDeskBarScrollButtonWidth - scroll_view_padding),
        bounds().y(), kDeskBarScrollButtonWidth, bounds().height());
  }

  gfx::Rect scroll_bounds(size());
  // Align with the overview grid in horizontal, so only horizontal insets are
  // needed here.
  scroll_bounds.Inset(gfx::Insets::VH(0, horizontal_padding));
  GetTopLevelViewWithContents().SetBoundsRect(scroll_bounds);
  if (!chromeos::features::AreOverviewSessionInitOptimizationsEnabled()) {
    // When the bar reaches its max possible size, it's size does not change,
    // but we still need to layout child UIs to their right positions.
    GetTopLevelViewWithContents().DeprecatedLayoutImmediately();
  }

  if (IsScrollingInitialized()) {
    UpdateScrollButtonsVisibility();
    UpdateGradientMask();
  }

  for (const auto& post_layout_operation : post_layout_operations) {
    post_layout_operation->Run();
  }
}

bool DeskBarViewBase::OnMousePressed(const ui::MouseEvent& event) {
  if (desk_activation_timer_.IsRunning()) {
    return false;
  }
  DeskNameView::CommitChanges(GetWidget());
  return false;
}

void DeskBarViewBase::OnGestureEvent(ui::GestureEvent* event) {
  if (desk_activation_timer_.IsRunning()) {
    return;
  }
  switch (event->type()) {
    case ui::EventType::kGestureLongPress:
    case ui::EventType::kGestureLongTap:
    case ui::EventType::kGestureTap:
    case ui::EventType::kGestureTapDown:
      DeskNameView::CommitChanges(GetWidget());
      break;

    default:
      break;
  }
}

void DeskBarViewBase::Init(aura::Window* desk_bar_widget_window) {
  CHECK(desk_bar_widget_window);
  // It's possible that window occlusion state change triggers some new windows
  // to show up during desk bar initialization process. It should not broadcast
  // the desk content update since desk mini view may not be ready. Please refer
  // to b/320530730.
  Desk::ScopedContentUpdateNotificationDisabler desks_scoped_notify_disabler(
      DesksController::Get()->desks(), /*notify_when_destroyed=*/false);

  UpdateNewMiniViews(/*initializing_bar_view=*/true,
                     /*expanding_bar_view=*/false);

  pending_post_layout_operations_.push_back(
      std::make_unique<ScrollForActiveMiniView>(this));

  hover_observer_ =
      std::make_unique<DeskBarHoverObserver>(this, desk_bar_widget_window);

  RecordDeskProfileAdoption();
}

bool DeskBarViewBase::IsZeroState() const {
  return state_ == DeskBarViewBase::State::kZero;
}

bool DeskBarViewBase::IsDraggingDesk() const {
  return drag_view_ != nullptr;
}

bool DeskBarViewBase::IsDeskNameBeingModified() const {
  if (!GetWidget() || !GetWidget()->IsActive()) {
    return false;
  }

  for (ash::DeskMiniView* mini_view : mini_views_) {
    if (mini_view->IsDeskNameBeingModified()) {
      return true;
    }
  }
  return false;
}

void DeskBarViewBase::ScrollToShowViewIfNecessary(const views::View* view) {
  if (!IsScrollingInitialized()) {
    return;
  }
  CHECK(base::Contains(contents_view_->children(), view));
  const gfx::Rect visible_bounds = scroll_view_->GetVisibleRect();
  const gfx::Rect view_bounds = view->bounds();
  const bool beyond_left = view_bounds.x() < visible_bounds.x();
  const bool beyond_right = view_bounds.right() > visible_bounds.right();
  auto* scroll_bar = scroll_view_->horizontal_scroll_bar();
  if (beyond_left) {
    scroll_view_->ScrollToPosition(
        scroll_bar, view_bounds.right() - scroll_view_->bounds().width());
  } else if (beyond_right) {
    scroll_view_->ScrollToPosition(scroll_bar, view_bounds.x());
  }
}

DeskMiniView* DeskBarViewBase::FindMiniViewForDesk(const Desk* desk) const {
  for (ash::DeskMiniView* mini_view : mini_views_) {
    if (mini_view->desk() == desk) {
      return mini_view;
    }
  }

  return nullptr;
}

int DeskBarViewBase::GetMiniViewIndex(const DeskMiniView* mini_view) const {
  auto iter = base::ranges::find(mini_views_, mini_view);
  return (iter == mini_views_.cend())
             ? -1
             : std::distance(mini_views_.cbegin(), iter);
}

void DeskBarViewBase::OnNewDeskButtonPressed(
    DesksCreationRemovalSource desks_creation_removal_source) {
  if (desk_activation_timer_.IsRunning()) {
    return;
  }
  auto* controller = DesksController::Get();
  if (!controller->CanCreateDesks()) {
    return;
  }

  base::UmaHistogramBoolean(type_ == Type::kDeskButton
                                ? kDeskButtonDeskBarNewDeskHistogramName
                                : kOverviewDeskBarNewDeskHistogramName,
                            true);

  pending_post_layout_operations_.push_back(
      std::make_unique<NewDeskButtonPressedScroll>(this));
  controller->NewDesk(desks_creation_removal_source);
}

void DeskBarViewBase::NudgeDeskName(int desk_index) {
  CHECK_LT(desk_index, static_cast<int>(mini_views_.size()));

  auto* name_view = mini_views_[desk_index]->desk_name_view();
  name_view->RequestFocus();

  if (type_ == Type::kOverview) {
    // If we're in tablet mode and there are no external keyboards, open up the
    // virtual keyboard.
    if (display::Screen::GetScreen()->InTabletMode() &&
        !HasExternalKeyboard()) {
      keyboard::KeyboardUIController::Get()->ShowKeyboard(/*lock=*/false);
    }
  }
}

void DeskBarViewBase::UpdateButtonsForSavedDeskGrid() {
  if (IsZeroState() || !saved_desk_util::ShouldShowSavedDesksOptions()) {
    return;
  }

  FindMiniViewForDesk(Shell::Get()->desks_controller()->active_desk())
      ->UpdateFocusColor();

  if (type_ == Type::kOverview && library_button_) {
    library_button_->set_paint_as_active(
        overview_grid_->IsShowingSavedDeskLibrary());
    library_button_->UpdateFocusState();
  }
}

void DeskBarViewBase::UpdateDeskButtonsVisibility() {
  const bool default_desk_button_new_visibility = IsZeroState();
  if (default_desk_button_new_visibility !=
      default_desk_button_->GetVisible()) {
    default_desk_button_->SetVisible(default_desk_button_new_visibility);
    contents_view_->InvalidateLayout();
  }

  UpdateNewDeskButtonLabelVisibility(
      new_desk_button_->state() == DeskIconButton::State::kActive,
      // If not attached to a widget yet, a layout get automatically run when
      // `Widget::SetContentsView()` is called.
      /*layout_if_changed=*/GetWidget());
  UpdateLibraryButtonVisibility();
}

void DeskBarViewBase::UpdateLibraryButtonVisibility() {
  if (!saved_desk_util::ShouldShowSavedDesksOptions()) {
    return;
  }

  const bool should_show_library_button = ShouldShowLibraryUi();
  DeskIconButton::State new_library_button_state = DeskIconButton::State::kZero;
  if (type_ == Type::kOverview && overview_grid_->IsShowingSavedDeskLibrary()) {
    new_library_button_state = DeskIconButton::State::kActive;
  } else if (state_ == State::kExpanded) {
    new_library_button_state = DeskIconButton::State::kExpanded;
  }

  // Lazy initialization will be the default when
  // `kOverviewSessionInitOptimizations` is launched.
  if (!chromeos::features::AreOverviewSessionInitOptimizationsEnabled() ||
      should_show_library_button) {
    GetOrCreateLibraryButton();
  }

  if (library_button_label_) {
    library_button_label_->SetVisible(should_show_library_button &&
                                      library_button_->state() ==
                                          DeskIconButton::State::kActive);
  }
  // If the visibility of the library button doesn't change, return early.
  if (!library_button_ ||
      (library_button_->GetVisible() == ShouldShowLibraryUi() &&
       library_button_->state() == new_library_button_state)) {
    return;
  }

  library_button_->SetVisible(should_show_library_button);
  if (should_show_library_button) {
    library_button_->UpdateState(new_library_button_state);
  }

  if (mini_views_.empty()) {
    return;
  }

  pending_post_layout_operations_.push_back(
      std::make_unique<LibraryButtonVisibilityAnimation>(this));
  contents_view_->InvalidateLayout();
}

void DeskBarViewBase::UpdateNewDeskButtonLabelVisibility(
    bool new_visibility,
    bool layout_if_changed) {
  const bool current_visibility =
      new_desk_button_label_ && new_desk_button_label_->GetVisible();
  if (chromeos::features::AreOverviewSessionInitOptimizationsEnabled()) {
    if (new_visibility) {
      GetOrCreateNewDeskButtonLabel().SetVisible(true);
    } else if (new_desk_button_label_) {
      new_desk_button_label_->SetVisible(false);
    }
  } else {
    GetOrCreateNewDeskButtonLabel().SetVisible(new_visibility);
  }

  if (new_visibility != current_visibility && layout_if_changed) {
    contents_view_->InvalidateLayout();
  }
}

void DeskBarViewBase::UpdateDeskIconButtonState(
    DeskIconButton* button,
    DeskIconButton::State target_state) {
  CHECK_NE(target_state, DeskIconButton::State::kZero);

  if (button->state() == target_state) {
    return;
  }

  button->UpdateState(target_state);
  pending_post_layout_operations_.push_back(
      std::make_unique<DeskIconButtonScaleAnimation>(this, button));
  contents_view_->InvalidateLayout();
}

void DeskBarViewBase::OnHoverStateMayHaveChanged() {
  for (ash::DeskMiniView* mini_view : mini_views_) {
    mini_view->UpdateDeskButtonVisibility();
  }
}

void DeskBarViewBase::OnGestureTap(const gfx::Rect& screen_rect,
                                   bool is_long_gesture) {
  if (desk_activation_timer_.IsRunning()) {
    return;
  }
  for (ash::DeskMiniView* mini_view : mini_views_) {
    mini_view->OnWidgetGestureTap(screen_rect, is_long_gesture);
  }
}

bool DeskBarViewBase::ShouldShowLibraryUi() {
  // Only update visibility when needed. This will save a lot of repeated work.
  if (library_ui_visibility_ == LibraryUiVisibility::kToBeChecked) {
    if (!saved_desk_util::ShouldShowSavedDesksOptions() ||
        display::Screen::GetScreen()->InTabletMode()) {
      library_ui_visibility_ = LibraryUiVisibility::kHidden;
    } else {
      auto* desk_model = Shell::Get()->saved_desk_delegate()->GetDeskModel();
      CHECK(desk_model);
      size_t saved_desk_count = desk_model->GetDeskTemplateEntryCount() +
                                desk_model->GetSaveAndRecallDeskEntryCount();
      library_ui_visibility_ = saved_desk_count ? LibraryUiVisibility::kVisible
                                                : LibraryUiVisibility::kHidden;
    }
  }

  return library_ui_visibility_ == LibraryUiVisibility::kVisible;
}

void DeskBarViewBase::SetDragDetails(const gfx::Point& screen_location,
                                     bool dragged_item_over_bar) {
  last_dragged_item_screen_location_ = screen_location;
  const bool old_dragged_item_over_bar = dragged_item_over_bar_;
  dragged_item_over_bar_ = dragged_item_over_bar;

  if (!old_dragged_item_over_bar && !dragged_item_over_bar) {
    return;
  }

  for (ash::DeskMiniView* mini_view : mini_views_) {
    mini_view->UpdateFocusColor();
  }

  if (DesksController::Get()->CanCreateDesks()) {
    new_desk_button_->UpdateFocusState();
  }
}

void DeskBarViewBase::HandlePressEvent(DeskMiniView* mini_view,
                                       const ui::LocatedEvent& event) {
  if (mini_view->is_animating_to_remove()) {
    return;
  }

  DeskNameView::CommitChanges(GetWidget());

  if (ui::EventTarget* target = event.target()) {
    gfx::PointF location = target->GetScreenLocationF(event);
    InitDragDesk(mini_view, location);
  }
}

void DeskBarViewBase::HandleLongPressEvent(DeskMiniView* mini_view,
                                           const ui::LocatedEvent& event) {
  if (mini_view->is_animating_to_remove()) {
    return;
  }

  DeskNameView::CommitChanges(GetWidget());

  // Initialize and start drag.
  gfx::PointF location = event.target()->GetScreenLocationF(event);
  InitDragDesk(mini_view, location);
  StartDragDesk(mini_view, location, event.IsMouseEvent());

  mini_view->OpenContextMenu(ui::MENU_SOURCE_LONG_PRESS);
}

void DeskBarViewBase::HandleDragEvent(DeskMiniView* mini_view,
                                      const ui::LocatedEvent& event) {
  // Do not perform drag if drag proxy is not initialized, or the mini view is
  // animating to be removed.
  if (!drag_proxy_ || mini_view->is_animating_to_remove()) {
    return;
  }

  mini_view->MaybeCloseContextMenu();

  gfx::PointF location = event.target()->GetScreenLocationF(event);

  // If the drag proxy is initialized, start the drag. If the drag started,
  // continue drag.
  switch (drag_proxy_->state()) {
    case DeskDragProxy::State::kInitialized:
      StartDragDesk(mini_view, location, event.IsMouseEvent());
      break;
    case DeskDragProxy::State::kStarted:
      ContinueDragDesk(mini_view, location);
      break;
    default:
      DUMP_WILL_BE_NOTREACHED();
  }
}

bool DeskBarViewBase::HandleReleaseEvent(DeskMiniView* mini_view,
                                         const ui::LocatedEvent& event) {
  // Do not end drag if the proxy is not initialized, or the mini view is
  // animating to be removed.
  if (!drag_proxy_ || mini_view->is_animating_to_remove()) {
    return false;
  }

  // If the drag didn't start, finalize the drag. Otherwise, end the drag and
  // snap back the desk.
  switch (drag_proxy_->state()) {
    case DeskDragProxy::State::kInitialized:
      FinalizeDragDesk();
      return false;
    case DeskDragProxy::State::kStarted:
      // During a mouse drag, if we touch any other mini view, since the other
      // mini view receives `EventType::kGestureEnd` event, hence `mini_view`
      // here might be different than `drag_view_`. Thus, we use `drag_view_`.
      // Please refer to b/296106746.
      EndDragDesk(drag_view_, /*end_by_user=*/true);
      break;
    default:
      NOTREACHED();
  }
  return true;
}

void DeskBarViewBase::OnActivateDeskTimer(const base::Uuid& uuid) {
  OnUiUpdateDone();

  auto* desk_controller = DesksController::Get();
  if (Desk* desk = desk_controller->GetDeskByUuid(uuid)) {
    desk_controller->ActivateDesk(
        desk, type_ == Type::kDeskButton
                  ? DesksSwitchSource::kDeskButtonMiniViewButton
                  : DesksSwitchSource::kMiniViewButton);
  }
}

void DeskBarViewBase::HandleClickEvent(DeskMiniView* mini_view) {
  // A timer to delay closing the desk bar.
  if (!ui::ScopedAnimationDurationScaleMode::is_zero()) {
    desk_activation_timer_.Start(
        FROM_HERE,
        ui::ScopedAnimationDurationScaleMode::duration_multiplier() *
            kAnimationDelayDuration,
        base::BindOnce(&DeskBarViewBase::OnActivateDeskTimer,
                       base::Unretained(this), mini_view->desk()->uuid()));
  } else {
    OnActivateDeskTimer(mini_view->desk()->uuid());
  }
}

void DeskBarViewBase::InitDragDesk(DeskMiniView* mini_view,
                                   const gfx::PointF& location_in_screen) {
  CHECK(!mini_view->is_animating_to_remove());

  // If another view is being dragged, then end the drag.
  if (drag_view_) {
    EndDragDesk(drag_view_, /*end_by_user=*/false);
  }

  drag_view_ = mini_view;

  gfx::PointF preview_origin_in_screen(
      drag_view_->GetPreviewBoundsInScreen().origin());
  const float init_offset_x =
      location_in_screen.x() - preview_origin_in_screen.x();

  // Create a drag proxy for the dragged desk.
  drag_proxy_ = std::make_unique<DeskDragProxy>(this, drag_view_, init_offset_x,
                                                window_occlusion_calculator_);
}

void DeskBarViewBase::StartDragDesk(DeskMiniView* mini_view,
                                    const gfx::PointF& location_in_screen,
                                    bool is_mouse_dragging) {
  CHECK(drag_view_);
  CHECK(drag_proxy_);
  CHECK_EQ(mini_view, drag_view_);
  CHECK(!mini_view->is_animating_to_remove());

  // Hide the dragged mini view.
  drag_view_->layer()->SetOpacity(0.0f);

  // Create a drag proxy widget, scale it up and move its x-coordinate according
  // to the x of `location_in_screen`.
  drag_proxy_->InitAndScaleAndMoveToX(location_in_screen.x());

  Shell::Get()->cursor_manager()->SetCursor(ui::mojom::CursorType::kGrabbing);

  // Fire a haptic event if necessary.
  if (is_mouse_dragging) {
    chromeos::haptics_util::PlayHapticTouchpadEffect(
        ui::HapticTouchpadEffect::kTick,
        ui::HapticTouchpadEffectStrength::kMedium);
  }
}

void DeskBarViewBase::ContinueDragDesk(DeskMiniView* mini_view,
                                       const gfx::PointF& location_in_screen) {
  CHECK(drag_view_);
  CHECK(drag_proxy_);
  CHECK_EQ(mini_view, drag_view_);
  CHECK(!mini_view->is_animating_to_remove());

  drag_proxy_->DragToX(location_in_screen.x());

  // Check if the desk is on the scroll arrow buttons. Do not determine move
  // index while scrolling, since the positions of the desks on bar keep varying
  // during this process.
  if (MaybeScrollByDraggedDesk()) {
    return;
  }

  const auto drag_view_iter = base::ranges::find(mini_views_, drag_view_);
  CHECK(drag_view_iter != mini_views_.cend());

  const int old_index = drag_view_iter - mini_views_.cbegin();

  const int drag_pos_screen_x = drag_proxy_->GetBoundsInScreen().origin().x();

  // Determine the target location for the desk to be reordered.
  const int new_index = DetermineMoveIndex(drag_pos_screen_x);

  if (old_index != new_index) {
    Shell::Get()->desks_controller()->ReorderDesk(old_index, new_index);
  }
}

void DeskBarViewBase::EndDragDesk(DeskMiniView* mini_view, bool end_by_user) {
  CHECK(drag_view_);
  CHECK(drag_proxy_);
  CHECK_EQ(mini_view, drag_view_);
  CHECK(!mini_view->is_animating_to_remove());

  base::UmaHistogramBoolean(type_ == Type::kDeskButton
                                ? kDeskButtonDeskBarReorderDeskHistogramName
                                : kOverviewDeskBarReorderDeskHistogramName,
                            true);

  // Update default desk names after dropping.
  Shell::Get()->desks_controller()->UpdateDesksDefaultNames();
  Shell::Get()->cursor_manager()->SetCursor(ui::mojom::CursorType::kPointer);

  // The desk action button tooltips may need an update.
  MaybeUpdateDeskActionButtonTooltips();

  // Stop scroll even if the desk is on the scroll arrow buttons.
  if (IsScrollingInitialized()) {
    left_scroll_button_->OnDeskHoverEnd();
    right_scroll_button_->OnDeskHoverEnd();
  }

  // If the reordering is ended by the user (release the drag), perform the
  // snapping back animation and scroll the bar to target position. If current
  // drag is ended due to the start of a new drag or the end of the overview,
  // directly finalize current drag.
  if (end_by_user) {
    ScrollToShowViewIfNecessary(drag_view_);
    drag_proxy_->SnapBackToDragView();
  } else {
    FinalizeDragDesk();
  }
}

void DeskBarViewBase::FinalizeDragDesk() {
  if (drag_view_) {
    drag_view_->layer()->SetOpacity(1.0f);
    drag_view_ = nullptr;
  }
  drag_proxy_.reset();
}

void DeskBarViewBase::OnDeskAdded(const Desk* desk, bool from_undo) {
  DeskNameView::CommitChanges(GetWidget());

  // If a desk is added while overview mode is exiting, then the overview grid
  // will have already been destroyed and we must not try to expand the bar.
  const bool is_expanding_bar_view =
      overview_grid() &&
      new_desk_button_->state() == DeskIconButton::State::kZero;
  UpdateNewMiniViews(/*initializing_bar_view=*/false, is_expanding_bar_view);
  MaybeUpdateDeskActionButtonTooltips();
  if (!DesksController::Get()->CanCreateDesks()) {
    new_desk_button_->SetEnabled(/*enabled=*/false);
  }
}

void DeskBarViewBase::OnDeskRemoved(const Desk* desk) {
  DeskNameView::CommitChanges(GetWidget());
  auto iter = base::ranges::find_if(
      mini_views_,
      [desk](DeskMiniView* mini_view) { return mini_view->desk() == desk; });

  // There are cases where a desk may be removed before the `desk_bar_view`
  // finishes initializing (i.e. removed on a separate root window before the
  // overview starting animation completes). In those cases, that mini_view
  // would not exist and the bar view will already be in the correct state so we
  // do not need to update the UI (crbug.com/1346154).
  if (iter == mini_views_.end()) {
    return;
  }

  new_desk_button_->SetEnabled(/*enabled=*/true);

  for (DeskMiniView* mini_view : mini_views_) {
    mini_view->UpdateDeskButtonVisibility();
  }

  // Remove the mini view from the list now. And remove it from its parent
  // after the animation is done.
  DeskMiniView* removed_mini_view = *iter;
  mini_views_.erase(iter);

  // End dragging desk if remove a dragged desk.
  if (drag_view_ == removed_mini_view) {
    EndDragDesk(removed_mini_view, /*end_by_user=*/false);
  }

  pending_post_layout_operations_.push_back(
      std::make_unique<RemoveDeskAnimation>(this, removed_mini_view));
  // There is desk removal animatiion for overview bar but not for desk button
  // desk bar.
  if (type_ == Type::kOverview) {
    contents_view_->InvalidateLayout();
    // Overview bar desk removal will preform mini view removal animation, while
    // desk button bar removes mini view immediately.
  } else {
    // Desk button bar does not have mini view removal animation, mini view will
    // disappear immediately. Desk button bar will shrink during desk removal.
    removed_mini_view->parent()->RemoveChildViewT(removed_mini_view);
    contents_view_->InvalidateLayout();
  }
}

void DeskBarViewBase::OnDeskReordered(int old_index, int new_index) {
  desks_util::ReorderItem(mini_views_, old_index, new_index);

  // Update the order of child views.
  auto* reordered_view = mini_views_[new_index].get();
  reordered_view->parent()->ReorderChildView(reordered_view, new_index);
  reordered_view->parent()->NotifyAccessibilityEvent(
      ax::mojom::Event::kTreeChanged, true);

  // Update the desk indices in the shortcut views.
  reordered_view->UpdateDeskButtonVisibility();
  mini_views_[old_index]->UpdateDeskButtonVisibility();

  pending_post_layout_operations_.push_back(
      std::make_unique<ReorderDeskAnimation>(this, old_index, new_index));
  contents_view_->InvalidateLayout();
}

void DeskBarViewBase::OnDeskActivationChanged(const Desk* activated,
                                              const Desk* deactivated) {
  for (ash::DeskMiniView* mini_view : mini_views_) {
    const Desk* desk = mini_view->desk();
    if (desk == activated || desk == deactivated) {
      mini_view->UpdateFocusColor();
    }
  }
}

void DeskBarViewBase::OnDeskNameChanged(const Desk* desk,
                                        const std::u16string& new_name) {
  MaybeUpdateDeskActionButtonTooltips();
}

void DeskBarViewBase::UpdateNewMiniViews(bool initializing_bar_view,
                                         bool expanding_bar_view) {
  TRACE_EVENT0("ui", "DeskBarViewBase::UpdateNewMiniViews");
  const absl::Cleanup scrolling_check = [this] { InitScrollingIfRequired(); };
  const auto& desks = DesksController::Get()->desks();
  if (initializing_bar_view) {
    UpdateDeskButtonsVisibility();
  }
  if (IsZeroState() && !expanding_bar_view) {
    return;
  }

  // This should not be called when a desk is removed.
  DCHECK_LE(mini_views_.size(), desks.size());

  // New mini views can be added at any index, so we need to iterate through and
  // insert new mini views in a position in `mini_views_` that corresponds to
  // their index in the `DeskController`'s list of desks.
  int mini_view_index = 0;
  std::vector<DeskMiniView*> new_mini_views;
  for (const auto& desk : desks) {
    if (!FindMiniViewForDesk(desk.get())) {
      DeskMiniView* mini_view = contents_view_->AddChildViewAt(
          std::make_unique<DeskMiniView>(this, root_, desk.get(),
                                         window_occlusion_calculator_),
          mini_view_index);
      mini_views_.insert(mini_views_.begin() + mini_view_index, mini_view);
      new_mini_views.push_back(mini_view);
    }
    ++mini_view_index;
  }

  // Only record for `initializing_bar_view` since that's what impacts the
  // presentation time and animation smoothness when entering overview.
  if (initializing_bar_view && type_ == Type::kOverview) {
    size_t total_layers_mirrored = 0;
    for (const auto& mini_view : mini_views_) {
      total_layers_mirrored +=
          mini_view->desk_preview()->GetNumLayersMirrored();
    }
    // From local testing, 16 chrome browser windows (which metrics show is
    // likely much more than what most users have) resulted in ~1000 layers.
    base::UmaHistogramCounts1000("Ash.Overview.DeskBarNumLayersMirrored",
                                 total_layers_mirrored);
  }

  if (expanding_bar_view) {
    SwitchToExpandedState();
    return;
  }

  if (new_desk_button_->state() == DeskIconButton::State::kActive) {
    // Make sure the new desk button is updated to expanded state from the
    // active state. This can happen when dropping the window on the new desk
    // button.
    new_desk_button_->UpdateState(DeskIconButton::State::kExpanded);
  }

  const gfx::Rect old_bar_bounds = this->GetBoundsInScreen();

  // Bar widget bounds may need an update. Please note, we pause layout here so
  // it does not do it twice.
  pause_layout_ = true;
  UpdateBarBounds();
  pause_layout_ = false;

  if (!initializing_bar_view) {
    pending_post_layout_operations_.push_back(
        std::make_unique<AddDeskAnimation>(this, old_bar_bounds,
                                           std::move(new_mini_views)));
  }
  contents_view_->InvalidateLayout();
}

void DeskBarViewBase::SwitchToExpandedState() {
  state_ = DeskBarViewBase::State::kExpanded;

  UpdateDeskButtonsVisibility();
  PerformZeroStateToExpandedStateMiniViewAnimation(this);

  MaybeRefreshOverviewGridBounds();
}

void DeskBarViewBase::OnUiUpdateDone() {
  if (on_update_ui_closure_for_testing_) {
    std::move(on_update_ui_closure_for_testing_).Run();
  }
}

DeskIconButton& DeskBarViewBase::GetOrCreateLibraryButton() {
  if (library_button_) {
    return *library_button_;
  }
  const int button_text_id =
      saved_desk_util::AreDesksTemplatesEnabled()
          ? IDS_ASH_DESKS_TEMPLATES_DESKS_BAR_BUTTON_LIBRARY
          : IDS_ASH_DESKS_TEMPLATES_DESKS_BAR_BUTTON_SAVED_FOR_LATER;

  CHECK(contents_view_);
  library_button_ =
      contents_view_->AddChildView(std::make_unique<DeskIconButton>(
          this, &kDesksTemplatesIcon, l10n_util::GetStringUTF16(button_text_id),
          cros_tokens::kCrosSysOnSecondaryContainer,
          cros_tokens::kCrosSysInversePrimary,
          /*initially_enabled=*/true,
          base::BindRepeating(&DeskBarViewBase::OnLibraryButtonPressed,
                              base::Unretained(this)),
          base::BindRepeating(&DeskBarViewBase::InitScrollingIfRequired,
                              base::Unretained(this))));
  library_button_label_ =
      contents_view_->AddChildView(std::make_unique<views::Label>());
  library_button_label_->SetFontList(
      TypographyProvider::Get()->ResolveTypographyToken(
          TypographyToken::kCrosAnnotation1));
  library_button_label_->SetPaintToLayer();
  library_button_label_->layer()->SetFillsBoundsOpaquely(false);
  return *library_button_;
}

views::Label& DeskBarViewBase::GetOrCreateNewDeskButtonLabel() {
  if (new_desk_button_label_) {
    return *new_desk_button_label_;
  }
  new_desk_button_label_ =
      contents_view_->AddChildView(std::make_unique<views::Label>());
  new_desk_button_label_->SetPaintToLayer();
  new_desk_button_label_->layer()->SetFillsBoundsOpaquely(false);
  return *new_desk_button_label_;
}

void DeskBarViewBase::UpdateBarBounds() {}

int DeskBarViewBase::GetFirstMiniViewXOffset() const {
  return mini_views_.empty() ? bounds().CenterPoint().x()
                             : mini_views_[0]->GetMirroredX();
}

base::flat_map<views::View*, int>
DeskBarViewBase::GetAnimatableViewsCurrentXMap() const {
  base::flat_map<views::View*, int> result;
  auto insert_view = [&](views::View* view) {
    if (view) {
      result.emplace(view, view->GetBoundsInScreen().x());
    }
  };

  for (ash::DeskMiniView* mini_view : mini_views_) {
    insert_view(mini_view);
  }
  insert_view(new_desk_button_);
  insert_view(library_button_);
  return result;
}

int DeskBarViewBase::DetermineMoveIndex(int location_screen_x) const {
  const int views_size = static_cast<int>(mini_views_.size());

  // We find the target position according to the x-axis coordinate of the
  // desks' center positions in screen in ascending order.
  for (int new_index = 0; new_index != views_size - 1; ++new_index) {
    auto* mini_view = mini_views_[new_index].get();

    // Note that we cannot directly use `GetBoundsInScreen`. Because we may
    // perform animation (transform) on mini views. The bounds gotten from
    // `GetBoundsInScreen` may be the intermediate bounds during animation.
    // Therefore, we transfer a mini view's origin from its parent level to
    // avoid the influence of its own transform.
    gfx::Point center_screen_pos = mini_view->GetMirroredBounds().CenterPoint();
    views::View::ConvertPointToScreen(mini_view->parent(), &center_screen_pos);
    if (location_screen_x < center_screen_pos.x()) {
      return new_index;
    }
  }

  return views_size - 1;
}

void DeskBarViewBase::UpdateScrollButtonsVisibility() {
  CHECK(IsScrollingInitialized());
  const gfx::Rect visible_bounds = scroll_view_->GetVisibleRect();
  left_scroll_button_->SetVisible(width() == GetAvailableBounds().width() &&
                                  visible_bounds.x() > 0);
  right_scroll_button_->SetVisible(width() == GetAvailableBounds().width() &&
                                   visible_bounds.right() <
                                       contents_view_->bounds().width());
}

void DeskBarViewBase::UpdateGradientMask() {
  CHECK(IsScrollingInitialized());
  const bool is_rtl = base::i18n::IsRTL();
  const bool is_left_scroll_button_visible = left_scroll_button_->GetVisible();
  const bool is_right_scroll_button_visible =
      right_scroll_button_->GetVisible();
  const bool is_left_visible_only =
      is_left_scroll_button_visible && !is_right_scroll_button_visible;

  bool should_show_start_gradient = false;
  bool should_show_end_gradient = false;
  // Show the both sides gradients during scroll if the corresponding scroll
  // button is visible. Otherwise, show the start/end gradient only in last page
  // and show the end/start gradient if there are contents beyond the right/left
  // side of the visible bounds with LTR/RTL layout.
  if (scroll_view_->is_scrolling()) {
    should_show_start_gradient =
        is_rtl ? is_right_scroll_button_visible : is_left_scroll_button_visible;
    should_show_end_gradient =
        is_rtl ? is_left_scroll_button_visible : is_right_scroll_button_visible;
  } else {
    should_show_start_gradient =
        is_rtl ? is_right_scroll_button_visible : is_left_visible_only;
    should_show_end_gradient =
        is_rtl ? is_left_visible_only : is_right_scroll_button_visible;
  }

  // The bounds of the start and end gradient will be the same regardless it is
  // LTR or RTL layout. While the `left_scroll_button_` will be changed from
  // left to right and `right_scroll_button_` will be changed from right to left
  // if it is RTL layout.

  // Horizontal linear gradient, from left to right.
  gfx::LinearGradient gradient_mask(/*angle=*/0);

  // Fraction of layer width that gradient will be applied to.
  float fade_position = should_show_start_gradient || should_show_end_gradient
                            ? static_cast<float>(kDeskBarGradientZoneLength) /
                                  scroll_view_->bounds().width()
                            : 0;

  // Clamp the `fade_position` value to ensure that it fits within the range of
  // linear gradient.
  fade_position = std::clamp(fade_position, 0.0f, 1.0f);

  // Left fade in section.
  if (should_show_start_gradient) {
    gradient_mask.AddStep(/*fraction=*/0, /*alpha=*/0);
    gradient_mask.AddStep(fade_position, 255);
  }
  // Right fade out section.
  if (should_show_end_gradient) {
    gradient_mask.AddStep((1 - fade_position), 255);
    gradient_mask.AddStep(1, 0);
  }

  scroll_view_->layer()->SetGradientMask(gradient_mask);
  scroll_view_->SchedulePaint();
}

void DeskBarViewBase::ScrollToPreviousPage() {
  CHECK(IsScrollingInitialized());
  ui::ScopedLayerAnimationSettings settings(
      contents_view_->layer()->GetAnimator());
  InitScrollContentsAnimationSettings(settings);
  scroll_view_->ScrollToPosition(
      scroll_view_->horizontal_scroll_bar(),
      GetAdjustedUncroppedScrollPosition(scroll_view_->GetVisibleRect().x() -
                                         scroll_view_->width()));
}

void DeskBarViewBase::ScrollToNextPage() {
  CHECK(IsScrollingInitialized());
  ui::ScopedLayerAnimationSettings settings(
      contents_view_->layer()->GetAnimator());
  InitScrollContentsAnimationSettings(settings);
  scroll_view_->ScrollToPosition(
      scroll_view_->horizontal_scroll_bar(),
      GetAdjustedUncroppedScrollPosition(scroll_view_->GetVisibleRect().x() +
                                         scroll_view_->width()));
}

int DeskBarViewBase::GetAdjustedUncroppedScrollPosition(int position) const {
  CHECK(IsScrollingInitialized());
  // Let the ScrollView handle it if the given `position` is invalid or it can't
  // be adjusted.
  if (position <= 0 ||
      position >= contents_view_->bounds().width() - scroll_view_->width()) {
    return position;
  }

  int adjusted_position = position;
  int i = 0;
  gfx::Rect mini_view_bounds;
  const int mini_views_size = static_cast<int>(mini_views_.size());
  for (; i < mini_views_size; i++) {
    mini_view_bounds = mini_views_[i]->bounds();

    // Return early if there is no desk preview cropped at the start position.
    if (mini_view_bounds.x() >= position) {
      return position - kDeskBarDeskPreviewViewFocusRingThicknessAndPadding;
    }

    if (mini_view_bounds.x() < position &&
        mini_view_bounds.right() > position) {
      break;
    }
  }

  CHECK_LT(i, mini_views_size);
  if ((position - mini_view_bounds.x()) < mini_view_bounds.width() / 2) {
    adjusted_position = mini_view_bounds.x();
  } else {
    adjusted_position = mini_view_bounds.right();
    if (i + 1 < mini_views_size) {
      adjusted_position = mini_views_[i + 1]->bounds().x();
    }
  }
  return adjusted_position -
         kDeskBarDeskPreviewViewFocusRingThicknessAndPadding;
}

void DeskBarViewBase::OnLibraryButtonPressed() {
  if (desk_activation_timer_.IsRunning()) {
    return;
  }
  RecordLoadSavedDeskLibraryHistogram();

  base::UmaHistogramBoolean(type_ == Type::kDeskButton
                                ? kDeskButtonDeskBarOpenLibraryHistogramName
                                : kOverviewDeskBarOpenLibraryHistogramName,
                            true);

  if (IsDeskNameBeingModified()) {
    DeskNameView::CommitChanges(GetWidget());
  }

  aura::Window* root = GetWidget()->GetNativeWindow()->GetRootWindow();
  OverviewSession* overview_session;
  if (overview_grid_) {
    overview_session = overview_grid_->overview_session();
  } else {
    OverviewController* overview_controller =
        Shell::Get()->overview_controller();
    bool is_overview_started =
        overview_controller &&
        overview_controller->StartOverview(OverviewStartAction::kDeskButton);
    // If overview refuses to start, do nothing.
    if (!is_overview_started) {
      return;
    }
    overview_session = overview_controller->overview_session();
  }
  overview_session->ShowSavedDeskLibrary(base::Uuid(), /*saved_desk_name=*/u"",
                                         root);
}

void DeskBarViewBase::MaybeUpdateDeskActionButtonTooltips() {
  auto* desk_controller = DesksController::Get();
  for (ash::DeskMiniView* mini_view : mini_views_) {
    auto* desk = mini_view->desk();
    if (desk->is_desk_being_removed()) {
      continue;
    }

    int desk_index = desk_controller->GetDeskIndex(desk);
    auto* desk_action_view = mini_view->desk_action_view();
    const std::u16string combine_desk_tooltip =
        desk_controller->GetCombineDesksTargetName(desk);
    const std::u16string close_desk_tooltip =
        desk->name().empty() && desk_index != -1
            ? desk_controller->GetDeskDefaultName(desk_index)
            : desk->name();
    // The combine desks button only exists if the feature is disabled. The
    // context menu button that would appear in its place does not need to
    // update its tooltip as it doesn't use a formatted string.
    if (!features::IsSavedDeskUiRevampEnabled()) {
      desk_action_view->combine_desks_button()->UpdateTooltip(
          combine_desk_tooltip);
    }
    desk_action_view->close_all_button()->UpdateTooltip(close_desk_tooltip);
  }
}

void DeskBarViewBase::InitScrollingIfRequired() {
  if (!scroll_view_ && IsScrollingRequired()) {
    InitScrolling();
  }
}

void DeskBarViewBase::InitScrolling() {
  CHECK(!scroll_view_);

  std::unique_ptr<views::View> scroll_view_contents =
      contents_view_ ? RemoveChildViewT(contents_view_)
                     : std::make_unique<views::View>();

  // Use layer scrolling so that the contents will paint on top of the parent,
  // which uses `SetPaintToLayer()`.
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  scroll_view_->layer()->SetMasksToBounds(true);
  scroll_view_->SetBackgroundColor(std::nullopt);
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  scroll_view_->SetTreatAllScrollEventsAsHorizontal(true);
  scroll_view_->SetAllowKeyboardScrolling(false);

  left_scroll_button_ = AddChildView(std::make_unique<ScrollArrowButton>(
      base::BindRepeating(&DeskBarViewBase::ScrollToPreviousPage,
                          base::Unretained(this)),
      /*is_left_arrow=*/true, this));
  left_scroll_button_->RemoveFromFocusList();
  right_scroll_button_ = AddChildView(std::make_unique<ScrollArrowButton>(
      base::BindRepeating(&DeskBarViewBase::ScrollToNextPage,
                          base::Unretained(this)),
      /*is_left_arrow=*/false, this));
  right_scroll_button_->RemoveFromFocusList();
  // If this is a desk button desk bar, the bar does not paint to a layer,
  // therefore, the scroll arrow buttons need to be painted.
  if (type_ == Type::kDeskButton) {
    left_scroll_button_->SetPaintToLayer();
    left_scroll_button_->layer()->SetFillsBoundsOpaquely(false);
    right_scroll_button_->SetPaintToLayer();
    right_scroll_button_->layer()->SetFillsBoundsOpaquely(false);
  }

  // Since we created a `ScrollView` with scrolling with layers enabled, it will
  // automatically create a layer for our contents.
  contents_view_ = scroll_view_->SetContents(std::move(scroll_view_contents));
  CHECK(contents_view_->layer());

  on_contents_scrolled_subscription_ =
      scroll_view_->AddContentsScrolledCallback(base::BindRepeating(
          &DeskBarViewBase::OnContentsScrolled, base::Unretained(this)));
  on_contents_scroll_ended_subscription_ =
      scroll_view_->AddContentsScrollEndedCallback(base::BindRepeating(
          &DeskBarViewBase::OnContentsScrollEnded, base::Unretained(this)));

  // If this is not attached to a widget yet, a layout will run automatically
  // when that does happen, so there's no need to call `InvalidateLayout()`.
  if (GetWidget()) {
    contents_view_->InvalidateLayout();
  }
}

bool DeskBarViewBase::IsScrollingRequired() const {
  CHECK(contents_view_);
  const int current_desk_bar_width = contents_view_->GetPreferredSize().width();
  const int available_width_for_desk_bar = GetAvailableBounds().width();
  // It might be ok for `scrolling_threshold` and `available_width_for_desk_bar`
  // to be the same, but a safety margin is added to the threshold. This
  // minimizes the chances of failing to initialize scrolling before it's
  // required, while still providing latency benefits to most users with desk
  // bar contents that are not even close to exceeding the width of the screen.
  const int scrolling_threshold = available_width_for_desk_bar * 0.75f;
  return current_desk_bar_width >= scrolling_threshold;
}

bool DeskBarViewBase::IsScrollingInitialized() const {
  return !!scroll_view_;
}

views::View& DeskBarViewBase::GetTopLevelViewWithContents() {
  CHECK(contents_view_);
  return scroll_view_ ? *scroll_view_ : *contents_view_;
}

void DeskBarViewBase::OnContentsScrolled() {
  UpdateScrollButtonsVisibility();
  UpdateGradientMask();
}

void DeskBarViewBase::OnContentsScrollEnded() {
  CHECK(IsScrollingInitialized());
  const gfx::Rect visible_bounds = scroll_view_->GetVisibleRect();
  const int current_position = visible_bounds.x();
  const int adjusted_position =
      GetAdjustedUncroppedScrollPosition(current_position);
  if (current_position != adjusted_position) {
    scroll_view_->ScrollToPosition(scroll_view_->horizontal_scroll_bar(),
                                   adjusted_position);
  }
  UpdateGradientMask();
}

bool DeskBarViewBase::MaybeScrollByDraggedDesk() {
  CHECK(drag_proxy_);

  const gfx::Rect proxy_bounds = drag_proxy_->GetBoundsInScreen();

  // If the desk proxy overlaps a scroll button, scroll the bar in the
  // corresponding direction.
  for (ScrollArrowButton* scroll_button : {
           left_scroll_button_,
           right_scroll_button_,
       }) {
    if (!IsScrollingInitialized()) {
      continue;
    }
    if (scroll_button->GetVisible() &&
        proxy_bounds.Intersects(scroll_button->GetBoundsInScreen())) {
      scroll_button->OnDeskHoverStart();
      return true;
    }
    scroll_button->OnDeskHoverEnd();
  }

  return false;
}

void DeskBarViewBase::MaybeRefreshOverviewGridBounds() {
  if (type_ == DeskBarViewBase::Type::kOverview &&
      overview_grid_->scoped_overview_wallpaper_clipper()) {
    CHECK(overview_grid_);
    overview_grid_->RefreshGridBounds(/*animate=*/true);
  }
}

void DeskBarViewBase::RecordDeskProfileAdoption() {
  // With regards to desk profiles, the user can be in one of these buckets:
  //  1. Conditions for selecting a user profile have not been met.
  //  2. Conditions are met, but the user has not actively selected a profile.
  //  3. The user has selected a profile.
  DeskProfilesUsageStatus status = DeskProfilesUsageStatus::kConditionsNotMet;
  if (DesksController::Get()->GetNumberOfDesks() > 1) {
    for (const auto& mini_view : mini_views_) {
      if (mini_view->desk() && mini_view->desk()->lacros_profile_id()) {
        // The user has actively selected a profile for at least one desk.
        status = DeskProfilesUsageStatus::kEnabled;
        break;
      }
      if (mini_view->desk_profiles_button()) {
        // The user has the option to pick a profile.
        status = DeskProfilesUsageStatus::kConditionsMet;
      }
    }
  }

  base::UmaHistogramEnumeration(kDeskProfilesUsageStatusHistogramName, status);
}

BEGIN_METADATA(DeskBarViewBase)
END_METADATA

}  // namespace ash
