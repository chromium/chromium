// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_TEST_APP_DATA_LOAD_WAITER_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_TEST_APP_DATA_LOAD_WAITER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"

namespace ash {

// Helper class for testing that application data was loaded.
class TestAppDataLoadWaiter : public KioskAppManagerObserver {
 public:
  TestAppDataLoadWaiter(KioskChromeAppManager* manager,
                        const std::string& app_id,
                        const std::string& version);

  TestAppDataLoadWaiter(const TestAppDataLoadWaiter&) = delete;
  TestAppDataLoadWaiter& operator=(const TestAppDataLoadWaiter&) = delete;

  ~TestAppDataLoadWaiter() override;

  void Wait();

  void WaitForAppData();

  bool loaded() const { return loaded_; }

 private:
  enum WaitType {
    WAIT_FOR_CRX_CACHE,
    WAIT_FOR_APP_DATA,
  };

  // KioskAppManagerObserver overrides:
  void OnKioskAppDataChanged(const std::string& app_id) override;
  void OnKioskAppDataLoadFailure(const std::string& app_id) override;
  void OnKioskExtensionLoadedInCache(const std::string& app_id) override;
  void OnKioskExtensionDownloadFailed(const std::string& app_id) override;

  bool IsAppDataLoaded();

  std::unique_ptr<base::RunLoop> runner_;
  raw_ptr<KioskChromeAppManager> manager_;
  WaitType wait_type_;
  bool loaded_;
  bool quit_;
  std::string app_id_;
  std::string version_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_TEST_APP_DATA_LOAD_WAITER_H_
