// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_BROWSER_SIGNIN_DETECTOR_H_
#define CHROME_BROWSER_DIPS_DIPS_BROWSER_SIGNIN_DETECTOR_H_

#include "base/memory/raw_ptr.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class DIPSService;

class DIPSBrowserSigninDetector : public signin::IdentityManager::Observer {
 public:
  DIPSBrowserSigninDetector(DIPSService* dips_service,
                            signin::IdentityManager* identity_manager);
  DIPSBrowserSigninDetector(const DIPSBrowserSigninDetector&) = delete;
  DIPSBrowserSigninDetector& operator=(const DIPSBrowserSigninDetector&) =
      delete;
  ~DIPSBrowserSigninDetector() override;

 private:
  // Begin signin::IdentityManager::Observer overrides:
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  // End signin::IdentityManager::Observer overrides.

  // Processes account |info| and records interaction(s) in the DIPS Database if
  // the account |info| is relevant.
  void RecordInteractionsIfRelevant(const AccountInfo& info);

  raw_ptr<DIPSService> dips_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_observation_{this};
};

#endif  //  CHROME_BROWSER_DIPS_DIPS_BROWSER_SIGNIN_DETECTOR_H_
