// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_STATUS_PROVIDER_UPDATER_STATUS_AND_VALUE_PROVIDER_H_
#define CHROME_BROWSER_POLICY_STATUS_PROVIDER_UPDATER_STATUS_AND_VALUE_PROVIDER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chrome/browser/policy/value_provider/policy_value_provider.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"

struct GoogleUpdateState;
struct GoogleUpdatePoliciesAndState;

class Profile;

namespace policy {
class PolicyMap;
}

// A status and value provider for Google Updater policies. Starts to load the
// policy values and status asynchronously during construction and
// notifies when the policies are loaded. GetStatus() and GetValues() will
// return empty if they're called before policies are loaded.
class UpdaterStatusAndValueProvider : public policy::PolicyStatusProvider,
                                      public policy::PolicyValueProvider {
 public:
  explicit UpdaterStatusAndValueProvider(Profile* profile);
  ~UpdaterStatusAndValueProvider() override;

  // policy::PolicyStatusProvider implementation.
  base::Value::Dict GetStatus() override;

  // policy::PolicyValueProvider implementation.
  base::Value::Dict GetValues() override;

  base::Value::Dict GetNames() override;

  void Refresh() override;

 private:
  void OnDomainReceived(std::string domain);

  void OnUpdaterPoliciesRefreshed(
      std::unique_ptr<GoogleUpdatePoliciesAndState> updater_policies_and_state);

  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<GoogleUpdateState> updater_status_;
  std::unique_ptr<policy::PolicyMap> updater_policies_;
  std::string domain_;
  raw_ptr<Profile> profile_;
  base::WeakPtrFactory<UpdaterStatusAndValueProvider> weak_factory_{this};
};

#endif  // CHROME_BROWSER_POLICY_STATUS_PROVIDER_UPDATER_STATUS_AND_VALUE_PROVIDER_H_
