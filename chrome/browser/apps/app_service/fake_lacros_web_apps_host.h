// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_FAKE_LACROS_WEB_APPS_HOST_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_FAKE_LACROS_WEB_APPS_HOST_H_

#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace apps {

// This class is a fake lacros web app publisher host that lives in
// lacros-chrome to test web app publishing from lacros. This class will
// be removed after the actual lacros web app host code created.
// This needs to be a class because this will also be the fake impl for
// the app service crosapi in the lacros side.
// TODO(crbug.com/1144877): Remove after the actual lacros web app host code
// created.
class FakeLacrosWebAppsHost : public crosapi::mojom::AppController {
 public:
  FakeLacrosWebAppsHost();
  ~FakeLacrosWebAppsHost() override;
  FakeLacrosWebAppsHost(const FakeLacrosWebAppsHost&) = delete;
  FakeLacrosWebAppsHost& operator=(const FakeLacrosWebAppsHost&) = delete;

  // Initialise and publish a fake app from the fake host for testing.
  void Init();

 private:
  // crosapi::mojom::AppController overrides.
  void Uninstall(const std::string& app_id,
                 apps::mojom::UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;

  mojo::Receiver<crosapi::mojom::AppController> receiver_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_FAKE_LACROS_WEB_APPS_HOST_H_
