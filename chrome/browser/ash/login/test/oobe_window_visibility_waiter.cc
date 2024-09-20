// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/oobe_window_visibility_waiter.h"

#include "base/run_loop.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/native_widget_types.h"

namespace ash {

OobeWindowVisibilityWaiter::OobeWindowVisibilityWaiter(bool target_visibilty)
    : target_visibility_(target_visibilty) {}

OobeWindowVisibilityWaiter::~OobeWindowVisibilityWaiter() = default;

void OobeWindowVisibilityWaiter::Wait() {
  aura::Window* window = GetWindow();
  if (!window && !target_visibility_)
    return;

  DCHECK(window);
  if (target_visibility_ == window->IsVisible())
    return;

  base::RunLoop run_loop;
  wait_stop_closure_ = run_loop.QuitClosure();
  window_observation_.Observe(window);
  run_loop.Run();
}

void OobeWindowVisibilityWaiter::OnWindowVisibilityChanged(aura::Window* window,
                                                           bool visible) {
  if (visible != target_visibility_)
    return;
  window_observation_.Reset();
  std::move(wait_stop_closure_).Run();
}

void OobeWindowVisibilityWaiter::OnWindowDestroyed(aura::Window* window) {
  DCHECK(!target_visibility_);
  window_observation_.Reset();
  std::move(wait_stop_closure_).Run();
}

aura::Window* OobeWindowVisibilityWaiter::GetWindow() {
  LoginDisplayHost* host = LoginDisplayHost::default_host();
  if (!host || !host->GetOobeWebContents())
    return nullptr;
  return host->GetOobeWebContents()->GetTopLevelNativeWindow();
}

}  // namespace ash
