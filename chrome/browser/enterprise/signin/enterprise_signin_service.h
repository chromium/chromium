// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_ENTERPRISE_SIGNIN_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_ENTERPRISE_SIGNIN_SERVICE_H_

#include <string>

#include "build/build_config.h"

#if !(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN))
#error EnterpriseSigninService should only be built on desktop platforms.
#endif  // !(BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN))

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

class Profile;

namespace enterprise_signin {

class EnterpriseSigninService : public BrowserListObserver,
                                public KeyedService,
                                public syncer::SyncServiceObserver {
 public:
  explicit EnterpriseSigninService(Profile* profile);

  ~EnterpriseSigninService() override;

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 private:
  void OnPrefChanged(const std::string& pref_name);

  void OpenOrActivateGaiaReauthTab();

  raw_ptr<Profile> profile_;
  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};

  PrefChangeRegistrar pref_change_registrar_;

  syncer::SyncService::TransportState last_transport_state_ =
      syncer::SyncService::TransportState::START_DEFERRED;
};

}  // namespace enterprise_signin

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_ENTERPRISE_SIGNIN_SERVICE_H_
