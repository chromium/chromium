// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_TEST_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_TEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"

class Profile;

namespace apps {

class AppServiceProxy;

// Helper class to initialize AppService in unit tests.
class AppServiceTest {
 public:
  AppServiceTest();
  ~AppServiceTest();

  void SetUp(Profile* profile);

  void UninstallAllApps(Profile* profile);

  std::string GetAppName(const std::string& app_id) const;

  // Allow AppService async callbacks to run.
  void WaitForAppService();

  // Flush mojo calls to allow AppService async callbacks to run.
  void FlushMojoCalls();

 private:
  AppServiceProxy* app_service_proxy_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppServiceTest);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_TEST_H_
