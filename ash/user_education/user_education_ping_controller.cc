// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_ping_controller.h"

#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/user_education/user_education_class_properties.h"
#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_owner.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/animation_sequence_block.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_tracker.h"

namespace ash {
namespace {

// The singleton instance owned by the `UserEducationController`.
UserEducationPingController* g_instance = nullptr;

// Animation.
constexpr auto kAnimationRepeatCount = 3u;

// Helpers ---------------------------------------------------------------------

// Comparable to `gfx::Rect::ClampToCenteredSize()` except max size is used
// instead of min size when determining the new rect.
gfx::Rect EnlargeToCenteredSize(const gfx::Rect& rect, const gfx::Size& size) {
  const int width = std::max(rect.width(), size.width());
  const int height = std::max(rect.height(), size.height());
  const int x = rect.x() + (rect.width() - width) / 2;
  const int y = rect.y() + (rect.height() - height) / 2;
  return gfx::Rect(x, y, width, height);
}

gfx::Size EnlargeToSquare(const gfx::Size& size) {
  const int max = std::max(size.width(), size.height());
  return gfx::Size(max, max);
}

gfx::Rect Inset(const gfx::Rect& rect, const gfx::Insets* insets) {
  gfx::Rect inset_rect(rect);
  if (insets) {
    inset_rect.Inset(*insets);
  }
  return inset_rect;
}

gfx::Transform ScaleAboutCenter(const ui::Layer* layer, float scale) {
  return gfx::GetScaleTransform(gfx::Rect(layer->size()).CenterPoint(), scale);
}

}  // namespace

// UserEducationPingController::Ping -------------------------------------------

class UserEducationPingController::Ping : public views::ViewObserver {
 public:
  explicit Ping(views::View* view)
      : view_tracker_(view),
        parent_(std::make_unique<ui::Layer>(ui::LAYER_NOT_DRAWN)),
        child_(std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR)) {
    CHECK(view->layer());

    // Name ping layers so that they are easy to identify in debugging/testing.
    parent_.layer()->SetName(kPingParentLayerName);
    child_.layer()->SetName(kPingChildLayerName);

    // Configure `child_` layer properties.
    child_.layer()->SetFillsBoundsOpaquely(false);
    OnViewThemeChanged(view);

    // Add ping layers to the layer tree below `view` layers. This is done so
    // that the ping appears to be beneath the associated `view` to the user.
    parent_.layer()->Add(child_.layer());
    view->AddLayerToRegion(parent_.layer(), views::LayerRegion::kBelow);
  }

  Ping(const Ping&) = delete;
  Ping& operator=(const Ping&) = delete;
  ~Ping() override = default;

  // Returns the view associated with this ping. Note that this may return
  // `nullptr` once the associated view has been destroyed.
  const views::View* view() const { return view_tracker_.view(); }

  // Starts the ping animation, invoking exactly one of either `ended_callback`
  // or `aborted_callback` on animation completion. Note that it is safe to
  // destroy `this` from either callback.
  void Start(base::OnceClosure ended_callback,
             base::OnceClosure aborted_callback) {
    // Prohibit calling `Start()` when a ping animation is already in progress
    // or after the associated `view()` has been destroyed.
    CHECK(ended_callback_.is_null());
    CHECK(aborted_callback_.is_null());
    CHECK(view_tracker_.view());

    // Cache and validate callbacks.
    ended_callback_ = std::move(ended_callback);
    aborted_callback_ = std::move(aborted_callback);
    CHECK(!ended_callback_.is_null());
    CHECK(!aborted_callback_.is_null());

    // Observe the associated `view()` while the ping animation is in progress
    // to keep ping layers in sync.
    view_observation_.Observe(view_tracker_.view());

    // Start the ping animation.
    Update();
  }

 private:
  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* view) override {
    // Update ping to stay in sync with `view` bounds.
    Update();
  }

  void OnViewIsDeleting(views::View* view) override {
    // There's nothing to ping once `view` is deleted. Note that aborting the
    // ping animation may result in the destruction of `this`.
    Abort();
  }

  void OnViewPropertyChanged(views::View* view,
                             const void* key,
                             int64_t old_value) override {
    // Update ping to stay in sync with requested insets.
    if (key == kPingInsetsKey) {
      Update();
    }
  }

  // TODO(http://b/281536915): Replace with semantic color.
  void OnViewThemeChanged(views::View* view) override {
    child_.layer()->SetColor(DarkLightModeController::Get()->IsDarkModeEnabled()
                                 ? SK_ColorWHITE
                                 : SK_ColorBLACK);
  }

  void OnViewVisibilityChanged(views::View* view,
                               views::View* starting_view) override {
    // There's nothing to ping once `view` is no longer drawn. Note that
    // aborting the ping animation may result in the destruction of `this`.
    if (!view->IsDrawn()) {
      Abort();
    }
  }

  // Aborts the ping animation, invoking the `aborted_callback_`. Note that
  // aborting the ping animation may result in the destruction of `this`.
  void Abort() {
    // Prohibit calling when a ping animation is not in progress.
    CHECK(!ended_callback_.is_null());
    CHECK(!aborted_callback_.is_null());

    // Abort the ping animation. May result in destruction of `this`.
    EndOrAbort(std::move(aborted_callback_));
  }

  // Ends the ping animation, invoking the `ended_callback_`. Note that ending
  // the ping animation may result in the destruction of `this`.
  void End() {
    // Prohibit calling when a ping animation is not in progress.
    CHECK(!ended_callback_.is_null());
    CHECK(!aborted_callback_.is_null());

    // End the ping animation. May result in destruction of `this`.
    EndOrAbort(std::move(ended_callback_));
  }

  // Ends/aborts the ping animation, invoking the appropriate callback. Note
  // that ending/aborting the ping animation may result in the destruction of
  // `this`.
  void EndOrAbort(base::OnceClosure ended_or_aborted_callback) {
    // Prohibit calling when a ping animation is not in progress.
    CHECK(ended_callback_.is_null() != aborted_callback_.is_null());

    // Prevent callbacks from running when stopping the ping animation.
    weak_ptr_factory_.InvalidateWeakPtrs();
    child_.layer()->GetAnimator()->StopAnimating();

    // We no longer need to observe the associated `view()` to keep ping layers
    // in sync once the ping animation has ended/aborted.
    view_observation_.Reset();

    // Reset both callbacks so that whichever does not correspond to the
    // `ended_or_aborted_callback` invoked by this method will never be invoked.
    // Note that whichever callback will be invoked has already been moved.
    ended_callback_.Reset();
    aborted_callback_.Reset();

    // May result in destruction of `this`.
    std::move(ended_or_aborted_callback).Run();
  }

  // Updates ping layers for the current state. Note that this causes preemption
  // of any preexisting ping animation and the start of a new ping animation.
  void Update() {
    // Prohibit calling `Update()` until `Start()` has been called. This should
    // only be possible prior to destruction of the associated `view()`.
    CHECK(!ended_callback_.is_null());
    CHECK(!aborted_callback_.is_null());
    CHECK(view());

    // Prevent animation callbacks from running on preempted animations.
    weak_ptr_factory_.InvalidateWeakPtrs();

    // Cache ping layers.
    ui::Layer* const parent = parent_.layer();
    ui::Layer* const child = child_.layer();

    // Match `parent` bounds to that of the associated `view()` layer. Because
    // `parent` was added as a top-level layer beneath the `view()` layer, they
    // are siblings and will share the same origin even if not explicitly set.
    parent->SetBounds(view()->layer()->bounds());

    // Set `child` bounds based on the size and center point of its `parent`.
    // Because `child` was added to the `parent` layer, it is not a top-level
    // layer beneath the `view()` layer and will therefore not be forced to
    // share the same origin. Note that `child` bounds respect ping insets and
    // are always square.
    gfx::Rect bounds(parent->size());
    bounds = Inset(bounds, view()->GetProperty(kPingInsetsKey));
    bounds = EnlargeToCenteredSize(bounds, EnlargeToSquare(bounds.size()));
    child->SetBounds(bounds);

    // Clip `child` to a circle.
    CHECK_EQ(bounds.width(), bounds.height());
    child->SetRoundedCornerRadius(gfx::RoundedCornersF(bounds.width() / 2.f));

    // Invoke the appropriate callback on ping animation ended/aborted. Note
    // that these callbacks will not be run for preempted animations.
    views::AnimationBuilder builder;
    builder
        .OnAborted(base::BindOnce(&Ping::Abort, weak_ptr_factory_.GetWeakPtr()))
        .OnEnded(base::BindOnce(&Ping::End, weak_ptr_factory_.GetWeakPtr()))
        .SetPreemptionStrategy(ui::LayerAnimator::PreemptionStrategy::
                                   IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

    // NOTE: This could alternatively have been implemented using a repeating
    // animation, but `OnWillRepeat()` callbacks are not run when the first
    // animation sequence block has zero duration, see http://crbug.com/1443543.
    views::AnimationSequenceBlock* block = &builder.Once();
    for (size_t i = 0u; i < kAnimationRepeatCount; ++i) {
      if (i > 0u) {
        block = &block->Then();
      }
      block = &block->SetDuration(base::TimeDelta())
                   .SetOpacity(child, 0.5f)
                   .SetTransform(child, ScaleAboutCenter(child, 0.f))
                   .Then()
                   .SetDuration(base::Seconds(2))
                   .SetOpacity(child, 0.f, gfx::Tween::ACCEL_0_80_DECEL_80)
                   .SetTransform(child, ScaleAboutCenter(child, 3.f),
                                 gfx::Tween::ACCEL_0_40_DECEL_100);
    }
  }

  // Tracks the `view()` associated with this ping to prevent UAF, since `this`
  // class does not require that the associated `view()` will outlive it.
  views::ViewTracker view_tracker_;

  // Owners for the ping layers which are added to the layer tree below `view()`
  // layers. This is done so that the ping appears to be beneath the associated
  // `view()` to the user. Note that top-level layers added below view layers
  // always share the same origin as the view layer, so a `child_` layer is
  // needed in order to achieve desired bounds for the ping.
  ui::LayerOwner parent_;
  ui::LayerOwner child_;

  // Callback which is invoked when the ping animation ends. Invoking this
  // callback may result in the destruction of `this`.
  base::OnceClosure ended_callback_;

  // Callback which is invoked when the ping animation aborts. Invoking this
  // callback may result in the destruction of `this`.
  base::OnceClosure aborted_callback_;

  // Observes the associated `view()` while a ping animation is in progress in
  // order to keep ping layers in sync.
  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};

  // Weak pointer factory whose weak pointers are invalidated during animation
  // preemption to prevent ended/aborted callbacks from being run prematurely.
  base::WeakPtrFactory<Ping> weak_ptr_factory_{this};
};

// UserEducationPingController -------------------------------------------------

UserEducationPingController::UserEducationPingController() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

UserEducationPingController::~UserEducationPingController() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
UserEducationPingController* UserEducationPingController::Get() {
  return g_instance;
}

// TODO(http://b/281536915): Expose ability to set ended/aborted callbacks.
bool UserEducationPingController::CreatePing(PingId ping_id,
                                             views::View* view) {
  // A ping is not created if a ping already exists for `ping_id` or `view`.
  for (const auto& [id, ping] : pings_by_id_) {
    if (id == ping_id || ping->view() == view) {
      return false;
    }
  }

  // A ping isn't created if `view` is not drawn.
  if (!view->IsDrawn()) {
    return false;
  }

  // Create a ping for `view`.
  auto entry = pings_by_id_.emplace(ping_id, std::make_unique<Ping>(view));

  // Destroy the ping when its animation is ended/aborted. Note that this also
  // ensures only one of either `ended_callback` or `aborted_callback` is run.
  auto [ended_callback, aborted_callback] = base::SplitOnceCallback(
      base::BindOnce([](UserEducationPingController* self,
                        PingId ping_id) { self->pings_by_id_.erase(ping_id); },
                     base::Unretained(this), ping_id));

  // Start the ping animation.
  entry.first->second->Start(std::move(ended_callback),
                             std::move(aborted_callback));

  // Indicate success.
  return true;
}

std::optional<PingId> UserEducationPingController::GetPingId(
    const views::View* view) const {
  for (const auto& [id, ping] : pings_by_id_) {
    if (ping->view() == view) {
      return id;
    }
  }
  return std::nullopt;
}

}  // namespace ash
