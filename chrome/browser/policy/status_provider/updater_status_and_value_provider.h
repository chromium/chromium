// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_STATUS_PROVIDER_UPDATER_STATUS_AND_VALUE_PROVIDER_H_
#define CHROME_BROWSER_POLICY_STATUS_PROVIDER_UPDATER_STATUS_AND_VALUE_PROVIDER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/google/google_update_policy_fetcher.h"
#include "chrome/browser/policy/value_provider/policy_value_provider.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"

class Profile;


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
  base::DictValue GetStatus() override;
  policy::mojom::StatusPtr GetStatusMojo() override;

  // policy::PolicyValueProvider implementation.
  base::DictValue GetValues() override;
  policy::PolicyGroupMojoMap GetValuesMojo() const override;

  base::DictValue GetNames() override;
  policy::PolicyGroupNamesMojoMap GetNamesMojo() const override;

  void Refresh() override;

 private:
  void DoRefresh();
  static std::string GetUpdaterAppId();

  void Init();

#if BUILDFLAG(IS_WIN)
  // Called when the Active Directory domain query completes.
  void OnDomainReceived(std::string domain);
#endif  // BUILDFLAG(IS_WIN)

  void OnUpdaterPoliciesRefreshed(
      std::vector<GoogleUpdatePoliciesAndState> results);

  SEQUENCE_CHECKER(sequence_checker_);
  std::optional<GoogleUpdateState> updater_status_;
  std::optional<policy::PolicyMap> updater_policies_;
  std::string domain_;
  raw_ptr<Profile> profile_;
  base::WeakPtrFactory<UpdaterStatusAndValueProvider> weak_factory_{this};
};

#endif  // CHROME_BROWSER_POLICY_STATUS_PROVIDER_UPDATER_STATUS_AND_VALUE_PROVIDER_H_
