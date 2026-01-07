// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_TEST_SUPPORT_FAKE_ACTOR_OVERLAY_PAGE_H_
#define CHROME_BROWSER_ACTOR_UI_TEST_SUPPORT_FAKE_ACTOR_OVERLAY_PAGE_H_

#include "chrome/browser/actor/ui/actor_overlay.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/gfx/geometry/point.h"

namespace actor::ui {

// Fake implementation for the ActorOverlayPage interface.
class FakeActorOverlayPage : public mojom::ActorOverlayPage {
 public:
  FakeActorOverlayPage();
  ~FakeActorOverlayPage() override;

  // Binds the receiver and returns the pending remote to be passed to the
  // factory.
  mojo::PendingRemote<mojom::ActorOverlayPage> BindAndGetRemote();

  // Waits for all messages currently in the pipe to be processed.
  void FlushForTesting();

  // Resets all counters to 0.
  void ResetCounters();

  // mojom::ActorOverlayPage implementation
  void SetScrimBackground(bool is_visible) override;
  void SetBorderGlowVisibility(bool is_visible) override;
  void MoveCursorTo(const gfx::Point& point,
                    MoveCursorToCallback callback) override;
  void SetTheme(mojom::ThemePtr theme) override;
  void TriggerClickAnimation(TriggerClickAnimationCallback callback) override;

  // Test accessors
  bool is_scrim_background_visible() const {
    return is_scrim_background_visible_;
  }
  int scrim_background_call_count() const {
    return set_scrim_background_call_count_;
  }
  bool is_border_glow_visible() const { return is_border_glow_visible_; }
  int border_glow_call_count() const { return set_border_glow_call_count_; }
  int theme_call_count() const { return theme_call_count_; }
  gfx::Point last_cursor_point() const { return last_cursor_point_; }
  int move_cursor_call_count() const { return move_cursor_call_count_; }
  int trigger_click_animation_call_count() {
    return trigger_click_animation_call_count_;
  }

 private:
  mojo::Receiver<mojom::ActorOverlayPage> receiver_{this};
  bool is_scrim_background_visible_ = false;
  int set_scrim_background_call_count_ = 0;
  bool is_border_glow_visible_ = false;
  int set_border_glow_call_count_ = 0;
  int theme_call_count_ = 0;
  gfx::Point last_cursor_point_;
  int move_cursor_call_count_ = 0;
  int trigger_click_animation_call_count_ = 0;
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_TEST_SUPPORT_FAKE_ACTOR_OVERLAY_PAGE_H_
