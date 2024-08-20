// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_HEADER_SERVICE_H_
#define CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_HEADER_SERVICE_H_

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/origin_trial_status_change_details.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace storage_access_api::trial {

// A profile-keyed service that maintains STORAGE_ACCESS_HEADER_ORIGIN_TRIAL
// content settings based on the state of the associated origin trial for a
// given `origin`-`partition_site` pair.
class StorageAccessHeaderService
    : public content::OriginTrialsControllerDelegate::Observer,
      public KeyedService {
 public:
  explicit StorageAccessHeaderService(content::BrowserContext* browser_context);
  ~StorageAccessHeaderService() override;
  StorageAccessHeaderService(const StorageAccessHeaderService&) = delete;
  StorageAccessHeaderService& operator=(const StorageAccessHeaderService&) =
      delete;

  // KeyedService overrides:
  void Shutdown() override;

  void UpdateSettingsForTesting(
      const content::OriginTrialStatusChangeDetails& details);

 private:
  // Updates `ContentSettingsForOneType::STORAGE_ACCESS_HEADER_TRIAL` to reflect
  // the status of the trial for `details.origin` (when embedded by
  // `details.partition_site`). If `details.match_subdomains` is true, a custom
  // scope is used for the content setting to match all subdomains of
  // `details.origin`.
  void UpdateSettings(const content::OriginTrialStatusChangeDetails& details);

  void SyncSettingsToNetworkService(HostContentSettingsMap* settings_map);

  // content::OriginTrialsControllerDelegate::Observer overrides:
  void OnStatusChanged(
      const content::OriginTrialStatusChangeDetails& details) override;
  void OnPersistedTokensCleared() override;
  std::string trial_name() override;

  const raw_ptr<content::OriginTrialsControllerDelegate>
      origin_trials_controller_;
  const raw_ref<content::BrowserContext> browser_context_;
};

}  // namespace storage_access_api::trial

#endif  // CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_HEADER_SERVICE_H_
