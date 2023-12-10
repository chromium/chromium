// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_app_host.h"

namespace arc {

FakeAppHost::FakeAppHost(
    ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>*
        app_connection_holder)
    : app_connection_holder_(app_connection_holder) {
  app_connection_holder_->SetHost(this);
}

FakeAppHost::~FakeAppHost() {
  app_connection_holder_->SetHost(nullptr);
}

void FakeAppHost::OnAppListRefreshed(std::vector<arc::mojom::AppInfoPtr> apps) {
}
void FakeAppHost::OnAppAddedDeprecated(arc::mojom::AppInfoPtr app) {}
void FakeAppHost::OnPackageAppListRefreshed(
    const std::string& package_name,
    std::vector<arc::mojom::AppInfoPtr> apps) {}
void FakeAppHost::OnInstallShortcut(arc::mojom::ShortcutInfoPtr shortcut) {}
void FakeAppHost::OnUninstallShortcut(const std::string& package_name,
                                      const std::string& intent_uri) {}
void FakeAppHost::OnPackageRemoved(const std::string& package_name) {}
void FakeAppHost::OnTaskCreated(int32_t task_id,
                                const std::string& package_name,
                                const std::string& activity,
                                const std::optional<std::string>& name,
                                const std::optional<std::string>& intent,
                                int32_t session_id) {}
void FakeAppHost::OnTaskDescriptionUpdated(
    int32_t task_id,
    const std::string& label,
    const std::vector<uint8_t>& icon_png_data) {}
void FakeAppHost::OnTaskDescriptionChanged(int32_t task_id,
                                           const std::string& label,
                                           arc::mojom::RawIconPngDataPtr icon,
                                           uint32_t primary_color,
                                           uint32_t status_bar_color) {}
void FakeAppHost::OnTaskDestroyed(int32_t task_id) {}
void FakeAppHost::OnTaskSetActive(int32_t task_id) {}
void FakeAppHost::OnNotificationsEnabledChanged(const std::string& package_name,
                                                bool enabled) {}
void FakeAppHost::OnPackageAdded(arc::mojom::ArcPackageInfoPtr package_info) {}
void FakeAppHost::OnPackageModified(
    arc::mojom::ArcPackageInfoPtr package_info) {}
void FakeAppHost::OnPackageListRefreshed(
    std::vector<arc::mojom::ArcPackageInfoPtr> packages) {}
void FakeAppHost::OnInstallationStarted(
    const std::optional<std::string>& package_name) {}
void FakeAppHost::OnInstallationFinished(
    arc::mojom::InstallationResultPtr result) {}
void FakeAppHost::OnInstallationProgressChanged(const std::string& package_name,
                                                float progress) {}
void FakeAppHost::OnInstallationActiveChanged(const std::string& package_name,
                                              bool active) {}

}  // namespace arc
