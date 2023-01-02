// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_MOCK_CLEANUP_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_MOCK_CLEANUP_HANDLER_H_

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_handler.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockCleanupHandler : public CleanupHandler {
 public:
  MockCleanupHandler();

  MockCleanupHandler(const MockCleanupHandler&) = delete;
  MockCleanupHandler& operator=(const MockCleanupHandler&) = delete;

  ~MockCleanupHandler() override;

  MOCK_METHOD(void, Cleanup, (CleanupHandlerCallback callback), (override));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_MOCK_CLEANUP_HANDLER_H_
