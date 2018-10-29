// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_MOCK_CHROME_CLEANER_CONTROLLER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_MOCK_CHROME_CLEANER_CONTROLLER_WIN_H_

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace safe_browsing {

class MockChromeCleanerController
    : public safe_browsing::ChromeCleanerController {
 public:
  MockChromeCleanerController();
  ~MockChromeCleanerController() override;

  // gmock does not support rvalue references in MOCK_METHOD. This method just
  // relays the argument to OnSwReporterReady_.
  void OnSwReporterReady(SwReporterInvocationSequence&& sequence) override;

  MOCK_CONST_METHOD0(state, State());
  MOCK_CONST_METHOD0(idle_reason, IdleReason());
  MOCK_METHOD1(SetLogsEnabled, void(bool));
  MOCK_CONST_METHOD0(logs_enabled, bool());
  MOCK_METHOD0(ResetIdleState, void());
  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD0(OnReporterSequenceStarted, void());
  MOCK_METHOD1(OnReporterSequenceDone, void(SwReporterInvocationResult));
  MOCK_METHOD0(RequestUserInitiatedScan, void());
  MOCK_METHOD1(MockedOnSwReporterReady, void(SwReporterInvocationSequence&));
  MOCK_METHOD1(Scan, void(const safe_browsing::SwReporterInvocation&));
  MOCK_METHOD3(ReplyWithUserResponse,
               void(Profile*, extensions::ExtensionService*, UserResponse));
  MOCK_METHOD0(Reboot, void());
  MOCK_METHOD0(IsAllowedByPolicy, bool());
  MOCK_METHOD0(IsReportingAllowedByPolicy, bool());
  MOCK_METHOD0(IsReportingManagedByPolicy, bool());
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_MOCK_CHROME_CLEANER_CONTROLLER_WIN_H_
