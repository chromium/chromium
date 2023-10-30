// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_CLIENT_IMPL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "chromeos/ash/components/growth/campaigns_manager_client.h"

namespace base {
class Version;
}

namespace growth {
class CampaignsManager;
}  // namespace growth

class CampaignsManagerClientImpl : public growth::CampaignsManagerClient {
 public:
  CampaignsManagerClientImpl();
  CampaignsManagerClientImpl(const CampaignsManagerClientImpl&) = delete;
  CampaignsManagerClientImpl& operator=(const CampaignsManagerClientImpl&) =
      delete;
  ~CampaignsManagerClientImpl() override;

  // growth::CampaignsManagerClient:
  void LoadCampaignsComponent(
      growth::CampaignComponentLoadedCallback callback) override;
  bool IsDeviceInDemoMode() const override;
  bool IsCloudGamingDevice() const override;
  bool IsFeatureAwareDevice() const override;
  const std::string& GetApplicationLocale() const override;
  const base::Version& GetDemoModeAppVersion() const override;

 private:
  void OnComponentDownloaded(
      growth::CampaignComponentLoadedCallback loaded_callback,
      component_updater::CrOSComponentManager::Error error,
      const base::FilePath& path);

  std::unique_ptr<growth::CampaignsManager> campaigns_manager_;

  base::WeakPtrFactory<CampaignsManagerClientImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_CLIENT_IMPL_H_
