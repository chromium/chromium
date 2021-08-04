// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LACROS_EXTENSION_APPS_CONTROLLER_H_
#define CHROME_BROWSER_LACROS_LACROS_EXTENSION_APPS_CONTROLLER_H_

#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

// This class is responsible for receiving AppController events from Ash, and
// implementing their effects.
//
// See LacrosExtensionAppsPublisher for the class responsible for sending events
// to Ash.
class LacrosExtensionAppsController : public crosapi::mojom::AppController {
 public:
  LacrosExtensionAppsController();
  ~LacrosExtensionAppsController() override;

  LacrosExtensionAppsController(const LacrosExtensionAppsController&) = delete;
  LacrosExtensionAppsController& operator=(
      const LacrosExtensionAppsController&) = delete;

  // This class does not receive events from ash until Initialize is called.
  // Tests may construct this class without using Initialize if the tests
  // directly call the AppController methods.
  void Initialize(mojo::Remote<crosapi::mojom::AppPublisher>& publisher);

  // crosapi::mojom::AppController
  // Public for testing.
  void Uninstall(const std::string& app_id,
                 apps::mojom::UninstallSource uninstall_source,
                 bool clear_site_data,
                 bool report_abuse) override;
  void PauseApp(const std::string& app_id) override;
  void UnpauseApp(const std::string& app_id) override;
  void GetMenuModel(const std::string& app_id,
                    GetMenuModelCallback callback) override;
  void LoadIcon(const std::string& app_id,
                apps::mojom::IconKeyPtr icon_key,
                apps::mojom::IconType icon_type,
                int32_t size_hint_in_dip,
                LoadIconCallback callback) override;
  void OpenNativeSettings(const std::string& app_id) override;
  void SetWindowMode(const std::string& app_id,
                     apps::mojom::WindowMode window_mode) override;

 private:
  // Mojo endpoint that's responsible for receiving messages from Ash.
  mojo::Receiver<crosapi::mojom::AppController> controller_;
};

#endif  // CHROME_BROWSER_LACROS_LACROS_EXTENSION_APPS_CONTROLLER_H_
