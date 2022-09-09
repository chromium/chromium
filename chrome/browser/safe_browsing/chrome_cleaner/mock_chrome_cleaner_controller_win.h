// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_MOCK_CHROME_CLEANER_CONTROLLER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_MOCK_CHROME_CLEANER_CONTROLLER_WIN_H_

#include <string>

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace safe_browsing {

class MockChromeCleanerController
    : public safe_browsing::ChromeCleanerController {
 public:
  MockChromeCleanerController();
  ~MockChromeCleanerController() override;

  // gmock does not support rvalue references in MOCK_METHOD. This method just
  // relays the arguments to MockedOnSwReporterReady_.
  void OnSwReporterReady(const std::string& prompt_seed,
                         SwReporterInvocationSequence&& sequence) override;

  MOCK_CONST_METHOD0(state, State());
  MOCK_CONST_METHOD0(idle_reason, IdleReason());
  MOCK_METHOD2(SetLogsEnabled, void(Profile*, bool));
  MOCK_CONST_METHOD1(logs_enabled, bool(Profile*));
  MOCK_METHOD0(ResetIdleState, void());
  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD1(HasObserver, bool(Observer*));
  MOCK_METHOD0(OnReporterSequenceStarted, void());
  MOCK_METHOD1(OnReporterSequenceDone, void(SwReporterInvocationResult));
  MOCK_METHOD1(RequestUserInitiatedScan, void(Profile*));
  MOCK_METHOD2(MockedOnSwReporterReady,
               void(const std::string&, SwReporterInvocationSequence&));
  MOCK_METHOD1(Scan, void(const safe_browsing::SwReporterInvocation&));
  MOCK_METHOD2(ReplyWithUserResponse, void(Profile*, UserResponse));
  MOCK_METHOD0(Reboot, void());
  MOCK_METHOD0(IsAllowedByPolicy, bool());
  MOCK_METHOD1(IsReportingManagedByPolicy, bool(Profile*));
  MOCK_METHOD0(GetIncomingPromptSeed, std::string());
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_MOCK_CHROME_CLEANER_CONTROLLER_WIN_H_
