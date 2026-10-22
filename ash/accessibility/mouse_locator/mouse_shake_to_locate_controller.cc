// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/mouse_locator/mouse_shake_to_locate_controller.h"

#include <memory>
#include <utility>

#include "ash/accessibility/accessibility_event_handler_manager.h"
#include "ash/shell.h"
#include "ash/utility/cursor_setter.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/wm/core/cursor_util.h"

namespace ash {

namespace {

// Duration of the animation that shrinks the cursor back to its original size
// once shaking stops.
constexpr base::TimeDelta kShrinkAnimationDuration = base::Milliseconds(100);

}  // namespace

// Captures the user's runtime cursor state and enlarges the cursor on
// construction, drives the shrink animation, and restores the original cursor
// state on destruction.
class MouseShakeToLocateController::ScopedEnlargedCursor
    : public gfx::AnimationDelegate {
 public:
  explicit ScopedEnlargedCursor(base::OnceClosure on_shrink_ended)
      : shrink_animation_(kShrinkAnimationDuration,
                          gfx::LinearAnimation::kDefaultFrameRate,
                          this),
        on_shrink_ended_(std::move(on_shrink_ended)) {
    auto* cursor_manager = Shell::Get()->cursor_manager();
    original_cursor_size_ = cursor_manager->GetCursorSize();
    original_large_cursor_size_ = cursor_manager->GetLargeCursorSizeInDip();

    EnlargeCursor();
    // The SetLargeCursorSizeInDip() call in EnlargeCursor() only takes effect
    // when the cursor size is ui::CursorSize::kLarge, so if the cursor is
    // normal size, we need to set it to kLarge.
    if (!WasEnlargedCursor()) {
      cursor_manager->SetCursorSize(ui::CursorSize::kLarge);
    }
    Shell::Get()->UpdateCursorCompositingEnabled();

    cursor_setter_ = std::make_unique<CursorSetter>();
    for (aura::Window* root_window : Shell::GetAllRootWindows()) {
      cursor_setter_->UpdateCursor(root_window,
                                   ui::mojom::CursorType::kPointer);
    }
  }

  ScopedEnlargedCursor(const ScopedEnlargedCursor&) = delete;
  ScopedEnlargedCursor& operator=(const ScopedEnlargedCursor&) = delete;

  ~ScopedEnlargedCursor() override {
    shrink_animation_.Stop();

    // Switch the cursor size first so that, when returning to `kNormal`, the
    // hidden large size is restored after `kNormal` is already visible.
    Shell::Get()->cursor_manager()->SetCursorSize(original_cursor_size_);
    Shell::Get()->SetLargeCursorSizeInDip(original_large_cursor_size_);

    cursor_setter_.reset();
    Shell::Get()->UpdateCursorCompositingEnabled();

    // Drop the cached Lottie cursor animation so Skia purges the intermediate
    // drop-shadow bitmaps accumulated in `SkImageFilterCache` during shrinking.
    wm::ClearCursorAnimationCache();
  }

  // Animates the cursor from its enlarged size back down to the target size.
  void ShrinkCursor() {
    shrink_start_size_ =
        Shell::Get()->cursor_manager()->GetLargeCursorSizeInDip();
    shrink_target_size_ = WasEnlargedCursor() ? original_large_cursor_size_
                                              : ui::kMinLargeCursorSize;
    CHECK_GE(shrink_start_size_, ui::kMinLargeCursorSize);
    CHECK_LE(shrink_start_size_, ui::kMaxLargeCursorSize);
    CHECK_GE(shrink_target_size_, ui::kMinLargeCursorSize);
    CHECK_LE(shrink_target_size_, ui::kMaxLargeCursorSize);

    if (shrink_start_size_ <= shrink_target_size_) {
      std::move(on_shrink_ended_).Run();
      return;
    }

    shrink_animation_.Start();
  }

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override {
    CHECK_EQ(animation, &shrink_animation_);
    const int size = gfx::Tween::IntValueBetween(
        animation->GetCurrentValue(), shrink_start_size_, shrink_target_size_);
    Shell::Get()->SetLargeCursorSizeInDip(size);
  }
  void AnimationEnded(const gfx::Animation* animation) override {
    CHECK_EQ(animation, &shrink_animation_);
    // Write the exact endpoint before switching from `kLarge` to `kNormal`.
    Shell::Get()->SetLargeCursorSizeInDip(shrink_target_size_);
    std::move(on_shrink_ended_).Run();
  }

 private:
  // Whether the cursor was already enlarged when the current shake began.
  bool WasEnlargedCursor() const {
    return original_cursor_size_ == ui::CursorSize::kLarge;
  }

  // Immediately sets the cursor to its enlarged shake size.
  void EnlargeCursor() {
    Shell::Get()->SetLargeCursorSizeInDip(WasEnlargedCursor()
                                              ? ui::kMaxLargeCursorSize
                                              : ui::kDefaultLargeCursorSize);
  }

  gfx::LinearAnimation shrink_animation_;
  int shrink_start_size_ = 0;
  int shrink_target_size_ = 0;

  ui::CursorSize original_cursor_size_ = ui::CursorSize::kNormal;
  int original_large_cursor_size_ = 0;

  // Overrides the cursor type to `kPointer` across all root windows while the
  // cursor is enlarged, and restores each window's previous cursor on reset.
  std::unique_ptr<CursorSetter> cursor_setter_;

  base::OnceClosure on_shrink_ended_;
};

MouseShakeToLocateController::MouseShakeToLocateController() = default;

MouseShakeToLocateController::~MouseShakeToLocateController() {
  Reset(enabled_);
}

void MouseShakeToLocateController::SetEnabled(bool enabled) {
  if (enabled_ == enabled) {
    return;
  }
  const bool was_enabled = enabled_;
  enabled_ = enabled;

  if (enabled_) {
    shake_observation_.Observe(&detector_);
    Shell::Get()->AddAccessibilityEventHandler(
        this,
        AccessibilityEventHandlerManager::HandlerType::kMouseShakeToLocate);
  } else {
    Reset(was_enabled);
  }
}

void MouseShakeToLocateController::CancelEffect() {
  scoped_enlarged_cursor_.reset();
  effect_state_ = EffectState::kNone;
}

void MouseShakeToLocateController::Reset(bool was_enabled) {
  if (was_enabled) {
    Shell::Get()->RemoveAccessibilityEventHandler(this);
  }
  shake_observation_.Reset();
  CancelEffect();
}

void MouseShakeToLocateController::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::EventType::kMouseMoved || event->IsSynthesized() ||
      !event->target()) {
    return;
  }

  const gfx::PointF location_in_screen =
      event->target()->GetScreenLocationF(*event);
  detector_.ProcessPosition(location_in_screen, event->time_stamp());
}

void MouseShakeToLocateController::OnMouseShakeStarted(
    const gfx::PointF& location_in_screen) {
  CHECK_EQ(effect_state_, EffectState::kNone);
  CHECK(!scoped_enlarged_cursor_);

  scoped_enlarged_cursor_ = std::make_unique<ScopedEnlargedCursor>(
      base::BindOnce(&MouseShakeToLocateController::OnShrinkAnimationEnded,
                     base::Unretained(this)));
  effect_state_ = EffectState::kEnlarged;
  base::RecordAction(base::UserMetricsAction(
      "Accessibility.CrosCursorShakeToLocate.Enlarged"));
}

void MouseShakeToLocateController::OnMouseShakeEnded() {
  // Ignored when the effect was already cancelled or is already shrinking.
  if (effect_state_ != EffectState::kEnlarged) {
    return;
  }
  CHECK(scoped_enlarged_cursor_);
  effect_state_ = EffectState::kShrinking;
  scoped_enlarged_cursor_->ShrinkCursor();
}

void MouseShakeToLocateController::OnShrinkAnimationEnded() {
  CancelEffect();
}

}  // namespace ash
