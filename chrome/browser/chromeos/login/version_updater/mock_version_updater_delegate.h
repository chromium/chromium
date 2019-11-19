// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_VERSION_UPDATER_MOCK_VERSION_UPDATER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_VERSION_UPDATER_MOCK_VERSION_UPDATER_DELEGATE_H_

#include <string>

#include "chrome/browser/chromeos/login/version_updater/version_updater.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockVersionUpdaterDelegate : public VersionUpdater::Delegate {
 public:
  MockVersionUpdaterDelegate();
  virtual ~MockVersionUpdaterDelegate();

  MOCK_METHOD1(UpdateInfoChanged,
               void(const VersionUpdater::UpdateInfo& update_info));
  MOCK_METHOD1(FinishExitUpdate, void(VersionUpdater::Result result));
  MOCK_METHOD0(OnWaitForRebootTimeElapsed, void());
  MOCK_METHOD0(PrepareForUpdateCheck, void());
  MOCK_METHOD3(UpdateErrorMessage,
               void(const NetworkPortalDetector::CaptivePortalStatus status,
                    const NetworkError::ErrorState& error_state,
                    const std::string& network_name));
  MOCK_METHOD0(ShowErrorMessage, void());
  MOCK_METHOD0(DelayErrorMessage, void());
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_VERSION_UPDATER_MOCK_VERSION_UPDATER_DELEGATE_H_
