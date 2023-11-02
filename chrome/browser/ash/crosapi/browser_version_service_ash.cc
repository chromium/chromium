// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_version_service_ash.h"

#include "base/ranges/algorithm.h"
#include "chrome/browser/ash/crosapi/browser_util.h"

namespace {

absl::optional<component_updater::ComponentInfo> GetComponent(
    const std::vector<component_updater::ComponentInfo>& components,
    const std::string& id) {
  auto it =
      base::ranges::find(components, id, &component_updater::ComponentInfo::id);

  if (it != components.end())
    return *it;

  return absl::nullopt;
}

}  // namespace

namespace crosapi {

BrowserVersionServiceAsh::BrowserVersionServiceAsh(
    component_updater::ComponentUpdateService* component_update_service)
    : component_update_service_(component_update_service) {
  // The component_updater_service may be null in tests.
  if (component_update_service_)
    component_update_service_->AddObserver(this);
}

BrowserVersionServiceAsh::~BrowserVersionServiceAsh() {
  // May be null in tests.
  if (component_update_service_) {
    // Removing an observer is a no-op if the observer wasn't added.
    component_update_service_->RemoveObserver(this);
  }
}

void BrowserVersionServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::BrowserVersionService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void BrowserVersionServiceAsh::AddBrowserVersionObserver(
    mojo::PendingRemote<mojom::BrowserVersionObserver> observer) {
  mojo::Remote<mojom::BrowserVersionObserver> remote(std::move(observer));

  // To avoid race conditions, trigger version notification on observer
  // registration.
  absl::optional<base::Version> browser_version = GetBrowserVersion();
  if (browser_version.has_value()) {
    remote->OnBrowserVersionInstalled(browser_version.value().GetString());
  }

  observers_.Add(std::move(remote));
}

void BrowserVersionServiceAsh::GetInstalledBrowserVersion(
    GetInstalledBrowserVersionCallback callback) {
  std::string version_str;

  auto browser_version = GetBrowserVersion();
  if (browser_version.has_value())
    version_str = browser_version.value().GetString();

  std::move(callback).Run(version_str);
}

void BrowserVersionServiceAsh::OnEvent(Events event, const std::string& id) {
  absl::optional<component_updater::ComponentInfo> component_info =
      GetComponent(component_update_service_->GetComponents(), id);
  // Check for notifications of the Lacros component being updated.
  if (event == Events::COMPONENT_UPDATED &&
      id == browser_util::GetLacrosComponentInfo().crx_id) {
    absl::optional<base::Version> browser_version = GetBrowserVersion();
    if (browser_version.has_value()) {
      std::string version_str = browser_version.value().GetString();
      for (auto& observer : observers_) {
        observer->OnBrowserVersionInstalled(version_str);
      }
    }
  }
}

absl::optional<base::Version> BrowserVersionServiceAsh::GetBrowserVersion() {
  absl::optional<component_updater::ComponentInfo> component_info =
      GetComponent(component_update_service_->GetComponents(),
                   browser_util::GetLacrosComponentInfo().crx_id);
  if (component_info.has_value()) {
    return component_info.value().version;
  }

  return absl::nullopt;
}

}  // namespace crosapi
