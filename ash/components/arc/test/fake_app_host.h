// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_APP_HOST_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_APP_HOST_H_

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/session/connection_holder.h"

namespace arc {

// For tests in //ash/components/arc that cannot use the real AppHost
// implementation in //chrome.
class FakeAppHost : public mojom::AppHost {
 public:
  explicit FakeAppHost(
      ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>*
          app_connection_holder);
  FakeAppHost(const FakeAppHost&) = delete;
  FakeAppHost& operator=(const FakeAppHost&) = delete;
  ~FakeAppHost() override;

  // mojom::AppHost overrides.
  void OnAppListRefreshed(std::vector<arc::mojom::AppInfoPtr> apps) override;
  void OnAppAddedDeprecated(arc::mojom::AppInfoPtr app) override;
  void OnPackageAppListRefreshed(
      const std::string& package_name,
      std::vector<arc::mojom::AppInfoPtr> apps) override;
  void OnInstallShortcut(arc::mojom::ShortcutInfoPtr shortcut) override;
  void OnUninstallShortcut(const std::string& package_name,
                           const std::string& intent_uri) override;
  void OnPackageRemoved(const std::string& package_name) override;
  void OnTaskCreated(int32_t task_id,
                     const std::string& package_name,
                     const std::string& activity,
                     const absl::optional<std::string>& name,
                     const absl::optional<std::string>& intent,
                     int32_t session_id) override;
  void OnTaskDescriptionUpdated(
      int32_t task_id,
      const std::string& label,
      const std::vector<uint8_t>& icon_png_data) override;
  void OnTaskDescriptionChanged(int32_t task_id,
                                const std::string& label,
                                arc::mojom::RawIconPngDataPtr icon,
                                uint32_t primary_color,
                                uint32_t status_bar_color) override;
  void OnTaskDestroyed(int32_t task_id) override;
  void OnTaskSetActive(int32_t task_id) override;
  void OnNotificationsEnabledChanged(const std::string& package_name,
                                     bool enabled) override;
  void OnPackageAdded(arc::mojom::ArcPackageInfoPtr package_info) override;
  void OnPackageModified(arc::mojom::ArcPackageInfoPtr package_info) override;
  void OnPackageListRefreshed(
      std::vector<arc::mojom::ArcPackageInfoPtr> packages) override;
  void OnInstallationStarted(
      const absl::optional<std::string>& package_name) override;
  void OnInstallationFinished(
      arc::mojom::InstallationResultPtr result) override;

 private:
  // The connection holder must outlive |this| object.
  ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>* const
      app_connection_holder_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_APP_HOST_H_
