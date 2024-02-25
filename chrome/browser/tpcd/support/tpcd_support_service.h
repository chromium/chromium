// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_SUPPORT_TPCD_SUPPORT_SERVICE_H_
#define CHROME_BROWSER_TPCD_SUPPORT_TPCD_SUPPORT_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/origin_trials_controller_delegate.h"

namespace content {
class BrowserContext;
}

class HostContentSettingsMap;

namespace tpcd::trial {

// A profile-keyed service that maintains TPCD_TRIAL content settings based on
// the state of the associated origin trial for a given
// `origin`-`partition_site` pair.
class TpcdTrialService
    : public content::OriginTrialsControllerDelegate::Observer,
      public KeyedService {
 public:
  explicit TpcdTrialService(content::BrowserContext* browser_context);
  ~TpcdTrialService() override;
  TpcdTrialService(const TpcdTrialService&) = delete;
  TpcdTrialService& operator=(const TpcdTrialService&) = delete;

  // KeyedService overrides:
  void Shutdown() override;

  void Update3pcdTrialSettingsForTesting(const url::Origin& origin,
                                         const std::string& partition_site,
                                         bool match_subdomains,
                                         bool enabled);

 private:
  // Updates `ContentSettingsForOneType::TPCD_TRIAL` to reflect
  // the status of the trial for `origin` (when embedded by `partition_site`).
  // If `match_subdomains` is true, a custom scope is used for the content
  // setting to match all subdomains of `origin`.
  void Update3pcdTrialSettings(const url::Origin& origin,
                               const std::string& partition_site,
                               bool match_subdomains,
                               bool enabled);
  void ClearTpcdTrialSettings();

  void SyncTpcdTrialSettingsToNetworkService(
      HostContentSettingsMap* settings_map);

  // content::OriginTrialsControllerDelegate::Observer overrides:
  void OnStatusChanged(const url::Origin& origin,
                       const std::string& partition_site,
                       bool match_subdomains,
                       bool enabled) override;
  void OnPersistedTokensCleared() override;
  std::string trial_name() override;

  raw_ptr<content::OriginTrialsControllerDelegate> ot_controller_;
  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace tpcd::trial

#endif  // CHROME_BROWSER_TPCD_SUPPORT_TPCD_SUPPORT_SERVICE_H_
