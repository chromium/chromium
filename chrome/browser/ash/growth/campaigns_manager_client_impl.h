// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_CLIENT_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/growth/metrics.h"
#include "chrome/browser/ash/growth/ui_action_performer.h"
#include "chromeos/ash/components/growth/campaigns_configuration_provider.h"
#include "chromeos/ash/components/growth/campaigns_manager_client.h"
#include "components/component_updater/ash/component_manager_ash.h"

namespace base {
class Version;
}

namespace growth {
class CampaignsManager;
}  // namespace growth

class CampaignsManagerClientImpl : public growth::CampaignsManagerClient,
                                   public UiActionPerformer::Observer {
 public:
  CampaignsManagerClientImpl();
  CampaignsManagerClientImpl(const CampaignsManagerClientImpl&) = delete;
  CampaignsManagerClientImpl& operator=(const CampaignsManagerClientImpl&) =
      delete;
  ~CampaignsManagerClientImpl() override;

  // growth::CampaignsManagerClient:
  void LoadCampaignsComponent(
      growth::CampaignComponentLoadedCallback callback) override;
  void AddOnTrackerInitializedCallback(
      growth::OnTrackerInitializedCallback callback) override;
  bool IsDeviceInDemoMode() const override;
  bool IsCloudGamingDevice() const override;
  bool IsFeatureAwareDevice() const override;
  bool IsAppIconOnShelf(const std::string& app_id) const override;
  const std::string& GetApplicationLocale() const override;
  const std::string& GetUserLocale() const override;

  // Returns the permanent country code stored for this client. See
  // chrome://translate-internal. Country code is in the format of lowercase ISO
  // 3166-1 alpha-2. Example: `us`, `br`, `in`.
  const std::string GetCountryCode() const override;
  const base::Version& GetDemoModeAppVersion() const override;
  growth::ActionMap GetCampaignsActions() override;
  void RegisterSyntheticFieldTrial(
      const std::string& trial_name,
      const std::string& group_name) const override;
  void ClearConfig(const std::map<std::string, std::string>& params) override;
  void RecordEvent(const std::string& event_name,
                   bool trigger_campaigns) override;
  bool WouldTriggerHelpUI(
      const std::map<std::string, std::string>& params) override;
  signin::IdentityManager* GetIdentityManager() const override;

  // UiActionPerformer::Observer:
  void OnReadyToLogImpression(int campaign_id,
                              std::optional<int> group_id,
                              bool should_log_cros_events) override;
  void OnDismissed(int campaign_id,
                   std::optional<int> group_id,
                   bool should_mark_dismissed,
                   bool should_log_cros_events) override;
  void OnButtonPressed(int campaign_id,
                       std::optional<int> group_id,
                       CampaignButtonId button_id,
                       bool should_mark_dismissed,
                       bool should_log_cros_events) override;

 private:
  void OnComponentDownloaded(
      growth::CampaignComponentLoadedCallback loaded_callback,
      component_updater::ComponentManagerAsh::Error error,
      const base::FilePath& path);
  void OnTrackerInitialized(growth::OnTrackerInitializedCallback callback,
                            bool init_success);
  void UpdateConfig(const std::map<std::string, std::string>& params);
  void RecordImpressionEvents(int campaign_id, std::optional<int> group_id);
  void RecordDismissalEvents(int campaign_id, std::optional<int> group_id);

  growth::CampaignsConfigurationProvider config_provider_;
  std::unique_ptr<growth::CampaignsManager> campaigns_manager_;

  // Reset before `campaigns_manager_`, because `this` observes one of the
  // performers owned by the manager.
  base::ScopedObservation<UiActionPerformer, UiActionPerformer::Observer>
      show_nudge_performer_observation_{this};

  // Reset before `campaigns_manager_`, because `this` observes one of the
  // performers owned by the manager.
  base::ScopedObservation<UiActionPerformer, UiActionPerformer::Observer>
      show_notification_performer_observation_{this};

  base::WeakPtrFactory<CampaignsManagerClientImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_GROWTH_CAMPAIGNS_MANAGER_CLIENT_IMPL_H_
