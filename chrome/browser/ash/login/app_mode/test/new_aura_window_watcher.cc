// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/test/new_aura_window_watcher.h"

#include "components/exo/wm_helper.h"

namespace ash {

NewAuraWindowWatcher::NewAuraWindowWatcher() {
  exo::WMHelper::GetInstance()->AddExoWindowObserver(this);
}

NewAuraWindowWatcher::~NewAuraWindowWatcher() {
  exo::WMHelper::GetInstance()->RemoveExoWindowObserver(this);
}

void NewAuraWindowWatcher::OnExoWindowCreated(aura::Window* window) {
  window_future_.SetValue(window);
}

aura::Window* NewAuraWindowWatcher::WaitForWindow() {
  return window_future_.Take();
}

}  // namespace ash
