// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_SERVICE_H_

#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/policy/status_provider/user_cloud_policy_status_provider.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/policy_export.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/models/image_model.h"

class Profile;

namespace gfx {
class Image;
}

namespace policy {

// This class gives information related to the browser's management state.
// For more information please read
// //components/policy/core/common/management/management_service.md
class BrowserManagementService : public ManagementService,
                                 public KeyedService,
                                 public PolicyStatusProvider::Observer {
 public:
  explicit BrowserManagementService(Profile* profile);
  ~BrowserManagementService() override;

  // Returns the management icon used to indicate profile level management.
  ui::ImageModel* GetManagementIconForProfile() override;
  gfx::Image* GetManagementIconForBrowser() override;

  void TriggerPolicyStatusChangedForTesting() override;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  void SetBrowserManagementIconForTesting(
      const gfx::Image& management_icon) override;
#endif

 private:
  // PolicyStatusProvider::Observer:
  void OnPolicyStatusChanged() override;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Starts listening to changes to policies that affect the enterprise label
  // and pill.
  void StartListeningToPrefChanges(Profile* profile);

  // Updates the management icon used to indicate profile level management.
  void UpdateManagementIconForProfile(Profile* profile);
  void UpdateManagementIconForBrowser(Profile* profile);
  void SetManagementIconForProfile(const gfx::Image& management_icon);
  void SetManagementIconForBrowser(const gfx::Image& management_icon);

  // Sets the enterprise label in the profile attribute storage and notifies
  // observers of the change.
  void UpdateEnterpriseLabelForProfile(Profile* profile);

  std::unique_ptr<UserCloudPolicyStatusProvider> provider_;
  PrefChangeRegistrar pref_change_registrar_;
  ui::ImageModel management_icon_for_profile_;
  gfx::Image management_icon_for_browser_;
  base::ScopedObservation<PolicyStatusProvider, PolicyStatusProvider::Observer>
      policy_status_provider_observations_{this};
  base::WeakPtrFactory<BrowserManagementService> weak_ptr_factory_{this};
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
};

}  // namespace policy

#endif  // CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_SERVICE_H_
