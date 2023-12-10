// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/login/app_mode/test/test_app_data_load_waiter.h"

namespace ash {

TestAppDataLoadWaiter::TestAppDataLoadWaiter(KioskChromeAppManager* manager,
                                             const std::string& app_id,
                                             const std::string& version)
    : runner_(nullptr),
      manager_(manager),
      wait_type_(WAIT_FOR_CRX_CACHE),
      loaded_(false),
      quit_(false),
      app_id_(app_id),
      version_(version) {
  manager_->AddObserver(this);
}

TestAppDataLoadWaiter::~TestAppDataLoadWaiter() {
  manager_->RemoveObserver(this);
}

void TestAppDataLoadWaiter::Wait() {
  wait_type_ = WAIT_FOR_CRX_CACHE;
  if (quit_) {
    return;
  }
  runner_ = std::make_unique<base::RunLoop>();
  runner_->Run();
}

void TestAppDataLoadWaiter::WaitForAppData() {
  wait_type_ = WAIT_FOR_APP_DATA;
  if (quit_ || IsAppDataLoaded()) {
    return;
  }
  runner_ = std::make_unique<base::RunLoop>();
  runner_->Run();
}

void TestAppDataLoadWaiter::OnKioskAppDataChanged(const std::string& app_id) {
  if (wait_type_ != WAIT_FOR_APP_DATA || app_id != app_id_ ||
      !IsAppDataLoaded()) {
    return;
  }

  loaded_ = true;
  quit_ = true;
  if (runner_.get()) {
    runner_->Quit();
  }
}

void TestAppDataLoadWaiter::OnKioskAppDataLoadFailure(
    const std::string& app_id) {
  if (wait_type_ != WAIT_FOR_APP_DATA || app_id != app_id_) {
    return;
  }

  loaded_ = false;
  quit_ = true;
  if (runner_.get()) {
    runner_->Quit();
  }
}

void TestAppDataLoadWaiter::OnKioskExtensionLoadedInCache(
    const std::string& app_id) {
  if (wait_type_ != WAIT_FOR_CRX_CACHE) {
    return;
  }

  std::string cached_version;
  base::FilePath file_path;
  if (!manager_->GetCachedCrx(app_id_, &file_path, &cached_version)) {
    return;
  }
  if (version_ != cached_version) {
    return;
  }
  loaded_ = true;
  quit_ = true;
  if (runner_.get()) {
    runner_->Quit();
  }
}

void TestAppDataLoadWaiter::OnKioskExtensionDownloadFailed(
    const std::string& app_id) {
  if (wait_type_ != WAIT_FOR_CRX_CACHE) {
    return;
  }

  loaded_ = false;
  quit_ = true;
  if (runner_.get()) {
    runner_->Quit();
  }
}

bool TestAppDataLoadWaiter::IsAppDataLoaded() {
  KioskChromeAppManager::App app;
  return manager_->GetApp(app_id_, &app) && !app.is_loading;
}

}  // namespace ash
