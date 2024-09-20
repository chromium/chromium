// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_BROWSER_SIGNIN_DETECTOR_H_
#define CHROME_BROWSER_DIPS_DIPS_BROWSER_SIGNIN_DETECTOR_H_

#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

struct AccountInfo;
class DIPSService;
class DIPSBrowserSigninDetectorFactory;

namespace content {
class BrowserContext;
}

// DIPSBrowserSigninDetector is a service because it depends on both DIPSService
// and IdentityManager. We need to be sure it gets shutdown before them.
//
// If, for example, we made DIPSService subclass SupportsUserData and attached
// DIPSBrowserSigninDetector to it as Data, we wouldn't be able to express the
// dependency of DIPSService on IdentityManager.
class DIPSBrowserSigninDetector : public KeyedService,
                                  signin::IdentityManager::Observer {
 public:
  DIPSBrowserSigninDetector(base::PassKey<DIPSBrowserSigninDetectorFactory>,
                            DIPSService* dips_service,
                            signin::IdentityManager* identity_manager);
  DIPSBrowserSigninDetector(const DIPSBrowserSigninDetector&) = delete;
  DIPSBrowserSigninDetector& operator=(const DIPSBrowserSigninDetector&) =
      delete;
  ~DIPSBrowserSigninDetector() override;

  static DIPSBrowserSigninDetector* Get(
      content::BrowserContext* browser_context);

 private:
  // Begin signin::IdentityManager::Observer overrides:
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  // End signin::IdentityManager::Observer overrides.

  // Begin KeyedService overrides:
  void Shutdown() override;
  // End KeyedService overrides.

  // Processes account |info| and records interaction(s) in the DIPS Database if
  // the account |info| is relevant.
  void RecordInteractionsIfRelevant(const AccountInfo& info);

  raw_ptr<DIPSService> dips_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_observation_{this};
};

#endif  // CHROME_BROWSER_DIPS_DIPS_BROWSER_SIGNIN_DETECTOR_H_
