// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_CLOUD_EXTERNAL_DATA_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_CLOUD_EXTERNAL_DATA_POLICY_HANDLER_H_

#include "chrome/browser/ash/policy/external_data/cloud_external_data_policy_observer.h"

namespace policy {

// Base class for handling per-user external resources like wallpaper or avatar
// images.
class CloudExternalDataPolicyHandler
    : public CloudExternalDataPolicyObserver::Delegate {
 public:
  CloudExternalDataPolicyHandler();

  CloudExternalDataPolicyHandler(const CloudExternalDataPolicyHandler&) =
      delete;
  CloudExternalDataPolicyHandler& operator=(
      const CloudExternalDataPolicyHandler&) = delete;

  virtual void RemoveForAccountId(const AccountId& account_id,
                                  base::OnceClosure on_removed) = 0;

  static AccountId GetAccountId(const std::string& user_id);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_CLOUD_EXTERNAL_DATA_POLICY_HANDLER_H_
