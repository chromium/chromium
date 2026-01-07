// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/test_support/fake_actor_overlay_page.h"

namespace actor::ui {

FakeActorOverlayPage::FakeActorOverlayPage() = default;

FakeActorOverlayPage::~FakeActorOverlayPage() = default;

mojo::PendingRemote<mojom::ActorOverlayPage>
FakeActorOverlayPage::BindAndGetRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void FakeActorOverlayPage::FlushForTesting() {
  receiver_.FlushForTesting();
}

void FakeActorOverlayPage::SetScrimBackground(bool is_visible) {
  is_scrim_background_visible_ = is_visible;
  set_scrim_background_call_count_++;
}

void FakeActorOverlayPage::SetBorderGlowVisibility(bool is_visible) {
  is_border_glow_visible_ = is_visible;
  set_border_glow_call_count_++;
}

void FakeActorOverlayPage::MoveCursorTo(const gfx::Point& point,
                                        MoveCursorToCallback callback) {
  last_cursor_point_ = point;
  move_cursor_call_count_++;
  // Simulate the WebUI replying immediately.
  std::move(callback).Run();
}

void FakeActorOverlayPage::SetTheme(mojom::ThemePtr theme) {
  theme_call_count_++;
}

void FakeActorOverlayPage::ResetCounters() {
  set_scrim_background_call_count_ = 0;
  set_border_glow_call_count_ = 0;
  move_cursor_call_count_ = 0;
  theme_call_count_ = 0;
}

void FakeActorOverlayPage::TriggerClickAnimation(
    TriggerClickAnimationCallback callback) {
  trigger_click_animation_call_count_++;
  // Simulate the WebUI replying once the animation is done.
  std::move(callback).Run();
}
}  // namespace actor::ui
