// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/cursor/cursor_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/paint_canvas.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "ui/gfx/presentation_feedback.h"

namespace ash {
namespace {

// Amount of time without cursor movement before entering stationary state.
const int kStationaryDelayMs = 500;

// Clamp velocity to this value.
const float kVelocityMax = 5000.0f;

// Interpolation factor used to compute responsive velocity. Valid range
// is 0.0 to 1.0, where 1.0 takes only current velocity into account.
const float kResponsiveVelocityFactor = 0.75f;

// Interpolation factor used to compute smooth velocity. Valid range
// is 0.0 to 1.0, where 1.0 takes only current velocity into account.
const float kSmoothVelocityFactor = 0.25f;

// Interpolation factor used to compute cursor movement. Valid range
// is 0.0 to 1.0, where 1.0 takes only smooth velocity into account.
const float kMovementFactor = 0.25f;

// Minimum movement for motion blur to be added.
const float kMinimumMovementForMotionBlur = 2.0f;

// Clamp motion blur sigma to this value.
const float kSigmaMax = 48.0f;

// Offset relative to VSYNC at which to request a redraw.
const int kVSyncOffsetMs = -4;

gfx::Vector2dF InterpolateBetween(const gfx::Vector2dF& start,
                                  const gfx::Vector2dF& end,
                                  float f) {
  return start + gfx::ScaleVector2d(end - start, f);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// CursorView::Painter

class CursorView::Painter : public viz::DelayBasedTimeSourceClient {
 public:
  using UpdateSurfaceCallback =
      base::RepeatingCallback<void(const gfx::Rect&, const gfx::Rect&, bool)>;

  Painter(CursorView* cursor_view,
          const gfx::Point& initial_location,
          bool is_motion_blur_enabled)
      : cursor_view_(cursor_view),
        is_motion_blur_enabled_(is_motion_blur_enabled),
        location_(initial_location),
        pending_location_(initial_location),
        ui_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
        paint_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
            {base::TaskPriority::USER_BLOCKING,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
        stationary_timer_(
            FROM_HERE,
            base::Milliseconds(kStationaryDelayMs),
            base::BindRepeating(&CursorView::Painter::StationaryOnPaintThread,
                                base::Unretained(this))) {
    // Detach sequence checker for future usage on paint thread.
    DETACH_FROM_SEQUENCE(paint_sequence_checker_);
  }
  ~Painter() override = default;

  void Init(UpdateSurfaceCallback update_surface_callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

    dsf_ = cursor_view_->GetWidget()
               ->GetNativeView()
               ->GetHost()
               ->device_scale_factor();
    update_surface_callback_ = std::move(update_surface_callback);
  }

  void Shutdown() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

    // Waits for the current paint (if any) to finish.
    base::AutoLock lock(lock_);

    cursor_view_ = nullptr;

    if (time_source_) {
      time_source_->SetClient(nullptr);
    }
    paint_task_runner_->DeleteSoon(FROM_HERE, this);
  }

  void SetCursorLocation(const gfx::Point& new_location) {
    // This is called from `CursorView::OnCursorLocationChanged` which could be
    // either on ui thread or evdev thread.

    // base::Unretained() is safe because `this` is deleted on the paint thread.
    paint_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CursorView::Painter::SetCursorLocationOnPaintThread,
                       base::Unretained(this), new_location));
  }

  void SetCursorImage(const gfx::ImageSkia& cursor_image,
                      const gfx::Size& cursor_size,
                      const gfx::Point& cursor_hotspot) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

    gfx::ImageSkia scaled_cursor_image = gfx::ImageSkia::CreateFrom1xBitmap(
        cursor_image.GetRepresentation(dsf_).GetBitmap());

    // base::Unretained() is safe because `this` is deleted on the paint thread.
    paint_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CursorView::Painter::SetCursorImageOnPaintThread,
                       base::Unretained(this), scaled_cursor_image, cursor_size,
                       cursor_hotspot));
  }

  void SetTimebaseAndInterval(base::TimeTicks timebase,
                              base::TimeDelta interval) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

    // base::Unretained() is safe because `this` is deleted on the paint thread.
    paint_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CursorView::Painter::SetTimebaseAndIntervalOnPaintThread,
            base::Unretained(this), timebase, interval));
  }

  // viz::DelayBasedTimeSourceClient overrides:
  void OnTimerTick() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(paint_sequence_checker_);

    gfx::Point old_location = location_;
    location_ = pending_location_;

    // Restart stationary timer if pointer location changed.
    if (location_ != old_location) {
      stationary_timer_.Reset();
    }

    base::TimeDelta interval = time_source_->Interval();
    // Compute velocity unless this is the first tick.
    if (time_source_->LastTickTime() == next_tick_time_) {
      // Velocity is pixels/second as interval might change.
      velocity_ = gfx::ScaleVector2d(old_location - location_,
                                     1.f / interval.InSecondsF());
      velocity_.SetToMin(gfx::Vector2dF(kVelocityMax, kVelocityMax));
    }

    // Save next tick time.
    next_tick_time_ = time_source_->NextTickTime();

    // Use "Complementary Filter" algorithm to determine velocity.
    // This allows us to be responsive in the short term and accurate
    // in the long term.
    responsive_velocity_ = InterpolateBetween(responsive_velocity_, velocity_,
                                              kResponsiveVelocityFactor);
    smooth_velocity_ =
        InterpolateBetween(smooth_velocity_, velocity_, kSmoothVelocityFactor);

    // Estimate movement over one time source (VSYNC) interval.
    gfx::Vector2dF movement = gfx::ScaleVector2d(
        InterpolateBetween(responsive_velocity_, smooth_velocity_,
                           kMovementFactor),
        interval.InSecondsF());

    float distance = movement.Length();
    if (is_motion_blur_enabled_ && distance >= kMinimumMovementForMotionBlur) {
      float sigma = std::min(distance / 3.f, kSigmaMax);

      // Create directional blur filter for |sigma|.
      motion_blur_filter_ = sk_make_sp<cc::BlurPaintFilter>(
          sigma, 0.f, SkTileMode::kDecal, nullptr);

      // Compute blur offset.
      motion_blur_offset_ =
          gfx::ScaleVector2d(movement, std::ceil(sigma * 3.f) / distance);

      // Determine angle of movement.
      SkScalar angle = SkScalarATan2(SkFloatToScalar(movement.y()),
                                     SkFloatToScalar(movement.x()));
      SkScalar cos_angle = SkScalarCos(angle);
      SkScalar sin_angle = SkScalarSin(angle);

      // Create transformation matrices for blur space.
      motion_blur_matrix_.setSinCos(-sin_angle, cos_angle);
      motion_blur_inverse_matrix_.setSinCos(sin_angle, cos_angle);
    } else {
      motion_blur_filter_.reset();
      responsive_velocity_ = gfx::Vector2dF();
      smooth_velocity_ = gfx::Vector2dF();
      time_source_->SetActive(false);
    }

    // Damage is the union of old and new cursor rectangles.
    gfx::Rect damage_rect = cursor_rect_;
    cursor_rect_ = CalculateCursorRectOnPaintThread();
    damage_rect.Union(cursor_rect_);

    // Paint damaged area now that all parameters have been determined.
    {
      // Ensures that `cursor_view_` is not destructed during lifetime of
      // `paint`.
      base::AutoLock lock(lock_);
      if (!cursor_view_) {
        return;
      }

      TRACE_EVENT1("ui", "CursorView::Paint", "damage_rect",
                   damage_rect.ToString());

      auto paint = cursor_view_->GetScopedPaint(damage_rect);

      cc::PaintCanvas* sk_canvas = paint->canvas().sk_canvas();
      sk_canvas->translate(SkIntToScalar(location_.x() - cursor_hotspot_.x()),
                           SkIntToScalar(location_.y() - cursor_hotspot_.y()));

      // Undo scaling because drawing bitmaps on a scaled canvas has artifacts.
      // The cursor image is scaled in `SetCursorImage` and drawn in 1x here.
      sk_canvas->scale(SkFloatToScalar(1 / dsf_));

      if (motion_blur_filter_) {
        sk_canvas->translate(SkIntToScalar(motion_blur_offset_.x()),
                             SkIntToScalar(motion_blur_offset_.y()));

        sk_canvas->concat(SkM44(motion_blur_inverse_matrix_));
        SkRect blur_rect = SkRect::MakeWH(SkIntToScalar(cursor_size_.width()),
                                          SkIntToScalar(cursor_size_.height()));
        motion_blur_matrix_.mapRect(&blur_rect);
        cc::PaintFlags flags;
        flags.setImageFilter(motion_blur_filter_);
        sk_canvas->saveLayer(blur_rect, flags);
        sk_canvas->concat(SkM44(motion_blur_matrix_));
        paint->canvas().DrawImageInt(cursor_image_, 0, 0);
        sk_canvas->restore();
      } else {
        // Fast path for when motion blur is not present.
        paint->canvas().DrawImageInt(cursor_image_, 0, 0);
      }
    }

    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(update_surface_callback_, cursor_rect_, damage_rect,
                       /*auto_refresh=*/stationary_timer_.IsRunning()));
  }

 private:
  void SetCursorLocationOnPaintThread(const gfx::Point& new_location) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(paint_sequence_checker_);
    if (location_ == new_location) {
      return;
    }

    pending_location_ = new_location;
    SetActiveOnPaintThread(true);
  }

  void SetCursorImageOnPaintThread(const gfx::ImageSkia& cursor_image,
                                   const gfx::Size& cursor_size,
                                   const gfx::Point& cursor_hotspot) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(paint_sequence_checker_);

    cursor_image_ = cursor_image;
    cursor_size_ = cursor_size;
    cursor_hotspot_ = cursor_hotspot;

    SetActiveOnPaintThread(true);
  }

  void StationaryOnPaintThread() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(paint_sequence_checker_);

    stationary_timer_.Stop();

    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(update_surface_callback_, cursor_rect_,
                                  /*damage_rect=*/gfx::Rect(),
                                  /*auto_refresh=*/false));
  }

  gfx::Rect CalculateCursorRectOnPaintThread() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(paint_sequence_checker_);

    if (cursor_size_.IsEmpty()) {
      return gfx::Rect();
    }

    SkRect cursor_rect = SkRect::MakeWH(SkIntToScalar(cursor_size_.width()),
                                        SkIntToScalar(cursor_size_.height()));

    if (motion_blur_filter_) {
      // Map cursor rectangle to blur space.
      motion_blur_matrix_.mapRect(&cursor_rect);

      // Expand rectangle using current blur filter.
      cc::PaintFlags flags;
      flags.setImageFilter(motion_blur_filter_);
      DCHECK(flags.ToSkPaint().canComputeFastBounds());
      flags.ToSkPaint().computeFastBounds(cursor_rect, &cursor_rect);

      // Map rectangle back to cursor space.
      motion_blur_inverse_matrix_.mapRect(&cursor_rect);

      // Add motion blur offset.
      cursor_rect.offset(SkIntToScalar(motion_blur_offset_.x()),
                         SkIntToScalar(motion_blur_offset_.y()));
    }

    cursor_rect.offset(SkIntToScalar(location_.x() - cursor_hotspot_.x()),
                       SkIntToScalar(location_.y() - cursor_hotspot_.y()));

    return gfx::ToEnclosingRect(gfx::SkRectToRectF(cursor_rect));
  }

  void SetActiveOnPaintThread(bool active) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(paint_sequence_checker_);

    // Create time source if it doesn't exist.
    if (!time_source_) {
      time_source_ =
          std::make_unique<viz::DelayBasedTimeSource>(paint_task_runner_.get());
      time_source_->SetClient(this);
    }
    time_source_->SetActive(active);
  }

  void SetTimebaseAndIntervalOnPaintThread(base::TimeTicks timebase,
                                           base::TimeDelta interval) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(paint_sequence_checker_);

    DCHECK(time_source_);
    time_source_->SetTimebaseAndInterval(
        timebase + base::Milliseconds(kVSyncOffsetMs), interval);
  }

  // Ensures painter is not destructed during paint.
  base::Lock lock_;

  raw_ptr<CursorView> cursor_view_;
  const bool is_motion_blur_enabled_;

  float dsf_ = 1.0f;

  gfx::Point location_;
  gfx::Point pending_location_;
  gfx::ImageSkia cursor_image_;
  gfx::Size cursor_size_;
  gfx::Point cursor_hotspot_;
  gfx::Rect cursor_rect_;
  base::TimeTicks next_tick_time_;
  gfx::Vector2dF velocity_;
  gfx::Vector2dF responsive_velocity_;
  gfx::Vector2dF smooth_velocity_;
  sk_sp<cc::PaintFilter> motion_blur_filter_;
  gfx::Vector2dF motion_blur_offset_;
  SkMatrix motion_blur_matrix_;
  SkMatrix motion_blur_inverse_matrix_;

  const scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> paint_task_runner_;

  std::unique_ptr<viz::DelayBasedTimeSource> time_source_;
  base::RetainingOneShotTimer stationary_timer_;
  SEQUENCE_CHECKER(paint_sequence_checker_);

  SEQUENCE_CHECKER(ui_sequence_checker_);
  UpdateSurfaceCallback update_surface_callback_;
};

////////////////////////////////////////////////////////////////////////////////
// CursorView, public:

CursorView::CursorView(const gfx::Point& initial_location,
                       bool is_motion_blur_enabled)
    : painter_(std::make_unique<Painter>(this,
                                         initial_location,
                                         is_motion_blur_enabled)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
}

CursorView::~CursorView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  ui::CursorController::GetInstance()->RemoveCursorObserver(this);

  // `painter_` schedules its destruction in Shutdown.
  painter_.release()->Shutdown();
}

// static
views::UniqueWidgetPtr CursorView::Create(const gfx::Point& initial_location,
                                          bool is_motion_blur_enabled,
                                          aura::Window* container) {
  CursorView* cursor_view =
      new CursorView(initial_location, is_motion_blur_enabled);
  auto widget = FastInkView::CreateWidgetWithContents(
      base::WrapUnique(cursor_view), container);

  // Initialize after FaskInkHost is set on `cursor_view` and it is attached to
  // a widget. So that it could get `buffer_to_screen_transform_` and be able to
  // initialize `painter_`.
  cursor_view->Init();

  return widget;
}

void CursorView::SetCursorImage(const gfx::ImageSkia& cursor_image,
                                const gfx::Size& cursor_size,
                                const gfx::Point& cursor_hotspot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  painter_->SetCursorImage(cursor_image, cursor_size, cursor_hotspot);
}

////////////////////////////////////////////////////////////////////////////////
// ui::CursorController::CursorObserver overrides:

void CursorView::OnCursorLocationChanged(const gfx::PointF& location) {
  gfx::PointF new_location_f = buffer_to_screen_transform_.MapPoint(location);
  gfx::Point new_location = gfx::ToRoundedPoint(new_location_f);

  painter_->SetCursorLocation(new_location);
}

////////////////////////////////////////////////////////////////////////////////
// ash::FastInkView overrides:

FastInkHost::PresentationCallback CursorView::GetPresentationCallback() {
  return base::BindRepeating(&CursorView::DidPresentCompositorFrame,
                             weak_ptr_factory_.GetWeakPtr());
}

////////////////////////////////////////////////////////////////////////////////
// CursorView, private:

void CursorView::Init() {
  buffer_to_screen_transform_ =
      host()->window_to_buffer_transform().GetCheckedInverse();

  painter_->Init(base::BindRepeating(&CursorView::UpdateSurface,
                                     weak_ptr_factory_.GetWeakPtr()));

  ui::CursorController::GetInstance()->AddCursorObserver(this);
}

void CursorView::DidPresentCompositorFrame(
    const gfx::PresentationFeedback& feedback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  painter_->SetTimebaseAndInterval(feedback.timestamp, feedback.interval);
}

}  // namespace ash
