// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_TEST_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_TEST_H_

#include <memory>
#include <string>
#include <vector>

#include "components/services/app_service/public/mojom/types.mojom-forward.h"
#include "ui/gfx/image/image_skia.h"

class Profile;

namespace apps {

class AppServiceProxyBase;

// Helper class to initialize AppService in unit tests.
class AppServiceTest {
 public:
  AppServiceTest();
  AppServiceTest(const AppServiceTest&) = delete;
  AppServiceTest& operator=(const AppServiceTest&) = delete;
  ~AppServiceTest();

  void SetUp(Profile* profile);

  void UninstallAllApps(Profile* profile);

  std::string GetAppName(const std::string& app_id) const;

  // Synchronously fetches the icon for |app_id| of type |app_type| for the
  // specified |size_hint_in_dp|, and blocks until the fetching is completed.
  gfx::ImageSkia LoadAppIconBlocking(apps::mojom::AppType app_type,
                                     const std::string& app_id,
                                     int32_t size_hint_in_dip);

  bool AreIconImageEqual(const gfx::ImageSkia& src, const gfx::ImageSkia& dst);

  // Allow AppService async callbacks to run.
  void WaitForAppService();

  // Flush mojo calls to allow AppService async callbacks to run.
  void FlushMojoCalls();

 private:
  AppServiceProxyBase* app_service_proxy_ = nullptr;

  Profile* profile_ = nullptr;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_TEST_H_
