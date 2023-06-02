// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_APP_SESSION_SERVICE_TEST_HELPER_H_
#define CHROME_BROWSER_SESSIONS_APP_SESSION_SERVICE_TEST_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/sessions/session_service_base_test_helper.h"

class Profile;
class AppSessionService;

class AppSessionServiceTestHelper : public SessionServiceBaseTestHelper {
 public:
  AppSessionServiceTestHelper();
  explicit AppSessionServiceTestHelper(Profile* profile);
  explicit AppSessionServiceTestHelper(AppSessionService* service);
  AppSessionServiceTestHelper(const AppSessionServiceTestHelper&) = delete;
  AppSessionServiceTestHelper& operator=(const AppSessionServiceTestHelper&) =
      delete;
  ~AppSessionServiceTestHelper() = default;

  void SetService(AppSessionService* service);
  AppSessionService* service() { return app_session_service_; }

 private:
  raw_ptr<AppSessionService, DanglingUntriaged> app_session_service_;
};

#endif  // CHROME_BROWSER_SESSIONS_APP_SESSION_SERVICE_TEST_HELPER_H_
