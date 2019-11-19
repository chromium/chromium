// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_EXTERNAL_DATA_HANDLERS_CLOUD_EXTERNAL_DATA_POLICY_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_EXTERNAL_DATA_HANDLERS_CLOUD_EXTERNAL_DATA_POLICY_HANDLER_H_

#include "chrome/browser/chromeos/policy/cloud_external_data_policy_observer.h"

namespace policy {

// Base class for handling per-user external resources like wallpaper or avatar
// images.
class CloudExternalDataPolicyHandler
    : public CloudExternalDataPolicyObserver::Delegate {
 public:
  CloudExternalDataPolicyHandler();

  virtual void RemoveForAccountId(const AccountId& account_id) = 0;

  static AccountId GetAccountId(const std::string& user_id);

 private:
  DISALLOW_COPY_AND_ASSIGN(CloudExternalDataPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_EXTERNAL_DATA_HANDLERS_CLOUD_EXTERNAL_DATA_POLICY_HANDLER_H_
