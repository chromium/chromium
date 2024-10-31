// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_FAKE_BROWSER_MANAGER_H_
#define CHROME_BROWSER_ASH_CROSAPI_FAKE_BROWSER_MANAGER_H_

#include "base/values.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"

namespace crosapi {

// A fake implementation of BrowserManager, used for testing.
class FakeBrowserManager : public BrowserManager {
 public:
  FakeBrowserManager();
  FakeBrowserManager(const FakeBrowserManager&) = delete;
  FakeBrowserManager& operator=(const FakeBrowserManager&) = delete;

  ~FakeBrowserManager() override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_FAKE_BROWSER_MANAGER_H_
