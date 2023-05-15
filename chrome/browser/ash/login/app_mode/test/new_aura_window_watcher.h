// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_NEW_AURA_WINDOW_WATCHER_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_NEW_AURA_WINDOW_WATCHER_H_

#include "base/test/test_future.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/window.h"

namespace ash {

// A helper class to wait until an aura window is created.
class NewAuraWindowWatcher : public exo::WMHelper::ExoWindowObserver {
 public:
  NewAuraWindowWatcher();

  ~NewAuraWindowWatcher() override;

  NewAuraWindowWatcher(const NewAuraWindowWatcher&) = delete;
  NewAuraWindowWatcher& operator=(const NewAuraWindowWatcher&) = delete;

  // `exo::WMHelper::ExoWindowObserver`
  void OnExoWindowCreated(aura::Window* window) override;

  // Waits until a new aura window is created.
  // The watch period starts when this object was created, not when this method
  // is called. In other words, this method may return immediately if an aura
  // window was already created before.
  aura::Window* WaitForWindow();

 private:
  base::test::TestFuture<aura::Window*> window_future_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_NEW_AURA_WINDOW_WATCHER_H_
