// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_TEST_UTIL_H_
#define CHROME_BROWSER_DICTATION_TEST_UTIL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/dictation/session_controller_delegate.h"
#include "chrome/browser/dictation/session_ui.h"
#include "chrome/browser/dictation/stream_provider.h"
#include "chrome/browser/dictation/target.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace dictation {

class MockStreamProvider : public StreamProvider {
 public:
  MockStreamProvider();
  ~MockStreamProvider() override;

  MOCK_METHOD(void, BindToTarget, (Target & target), (override));
  MOCK_METHOD(void, Stop, (), (override));
};

class MockSessionUi : public SessionUi {
 public:
  MockSessionUi();
  ~MockSessionUi() override;
};

class MockSessionControllerDelegate : public SessionControllerDelegate {
 public:
  MockSessionControllerDelegate();
  ~MockSessionControllerDelegate() override;

  MOCK_METHOD(std::unique_ptr<StreamProvider>,
              CreateStreamProvider,
              (SessionController & controller),
              (const, override));
  MOCK_METHOD(std::unique_ptr<SessionUi>,
              CreateUi,
              (SessionController & controller),
              (const, override));
  MOCK_METHOD(void, EndSession, (), (override));
};

class MockTarget : public Target {
 public:
  MockTarget();
  ~MockTarget() override;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_TEST_UTIL_H_
