// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_version_service_ash.h"

#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chromeos/ash/components/standalone_browser/channel_util.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"

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
  remote->OnBrowserVersionInstalled(GetLatestLaunchableBrowserVersion());

  observers_.Add(std::move(remote));
}

void BrowserVersionServiceAsh::GetInstalledBrowserVersion(
    GetInstalledBrowserVersionCallback callback) {
  std::move(callback).Run(GetLatestLaunchableBrowserVersion());
}

const BrowserVersionServiceAsh::Delegate*
BrowserVersionServiceAsh::GetDelegate() const {
  return delegate_for_testing_
             ? delegate_for_testing_.get()
             : crosapi::BrowserManager::Get()->version_service_delegate();
}

void BrowserVersionServiceAsh::OnEvent(
    const update_client::CrxUpdateItem& item) {
  // Check for notifications of the Lacros component being updated.
  if (item.state != update_client::ComponentState::kUpdated ||
      item.id != ash::standalone_browser::GetLacrosComponentInfo().crx_id ||
      !GetDelegate()->IsNewerBrowserAvailable()) {
    return;
  }

  for (auto& observer : observers_)
    observer->OnBrowserVersionInstalled(GetLatestLaunchableBrowserVersion());
}

std::string BrowserVersionServiceAsh::GetLatestLaunchableBrowserVersion()
    const {
  return GetDelegate()->GetLatestLaunchableBrowserVersion().GetString();
}

}  // namespace crosapi
