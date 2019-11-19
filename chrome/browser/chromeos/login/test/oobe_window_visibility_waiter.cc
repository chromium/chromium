// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/oobe_window_visibility_waiter.h"

#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {

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
  window_observer_.Add(window);
  run_loop.Run();
}

void OobeWindowVisibilityWaiter::OnWindowVisibilityChanged(aura::Window* window,
                                                           bool visible) {
  if (visible != target_visibility_)
    return;
  window_observer_.RemoveAll();
  std::move(wait_stop_closure_).Run();
}

void OobeWindowVisibilityWaiter::OnWindowDestroyed(aura::Window* window) {
  DCHECK(!target_visibility_);
  window_observer_.RemoveAll();
  std::move(wait_stop_closure_).Run();
}

aura::Window* OobeWindowVisibilityWaiter::GetWindow() {
  LoginDisplayHost* host = LoginDisplayHost::default_host();
  if (!host || !host->GetOobeWebContents())
    return nullptr;
  return host->GetOobeWebContents()->GetTopLevelNativeWindow();
}

}  // namespace chromeos
