// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/android_sms/fake_android_sms_app_manager.h"

namespace ash {
namespace android_sms {

FakeAndroidSmsAppManager::FakeAndroidSmsAppManager() = default;

FakeAndroidSmsAppManager::~FakeAndroidSmsAppManager() = default;

void FakeAndroidSmsAppManager::SetInstalledAppUrl(
    const std::optional<GURL>& url) {
  if (url == url_)
    return;

  url_ = url;
  NotifyInstalledAppUrlChanged();
}

std::optional<GURL> FakeAndroidSmsAppManager::GetCurrentAppUrl() {
  return url_;
}

}  // namespace android_sms
}  // namespace ash
