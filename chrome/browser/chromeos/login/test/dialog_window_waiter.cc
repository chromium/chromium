// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/dialog_window_waiter.h"

#include "ui/aura/env.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

DialogWindowWaiter::DialogWindowWaiter(const base::string16& dialog_title)
    : dialog_title_(dialog_title) {
  aura::Env::GetInstance()->AddObserver(this);
}

DialogWindowWaiter::~DialogWindowWaiter() {
  aura::Env::GetInstance()->RemoveObserver(this);
  window_observer_.RemoveAll();
  // Explicitly close any help app dialogs still open to prevent flaky errors in
  // browser tests. Remove when crbug.com/951828 is fixed.
  for (aura::Window* dialog_window : dialog_windows_)
    views::Widget::GetWidgetForNativeView(dialog_window)->CloseNow();
}

void DialogWindowWaiter::Wait() {
  run_loop_.Run();
}

void DialogWindowWaiter::OnWindowInitialized(aura::Window* window) {
  DCHECK(!window_observer_.IsObserving(window));
  window_observer_.Add(window);
}

void DialogWindowWaiter::OnWindowDestroyed(aura::Window* window) {
  if (window_observer_.IsObserving(window))
    window_observer_.Remove(window);
  dialog_windows_.erase(window);
}

void DialogWindowWaiter::OnWindowVisibilityChanged(aura::Window* window,
                                                   bool visible) {
  if (window->GetTitle() != dialog_title_)
    return;

  dialog_windows_.insert(window);
  if (visible) {
    run_loop_.Quit();
  }
}

}  // namespace chromeos
