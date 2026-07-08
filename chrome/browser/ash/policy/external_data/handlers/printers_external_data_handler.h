// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_PRINTERS_EXTERNAL_DATA_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_PRINTERS_EXTERNAL_DATA_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "chrome/browser/ash/policy/external_data/cloud_external_data_policy_observer.h"

class PrefService;

namespace policy {

class PrintersExternalDataHandler
    : public CloudExternalDataPolicyObserver::Delegate {
 public:
  // `local_state` must not be nullptr and must outlive this object.
  explicit PrintersExternalDataHandler(PrefService* local_state);
  PrintersExternalDataHandler(const PrintersExternalDataHandler&) = delete;
  PrintersExternalDataHandler& operator=(const PrintersExternalDataHandler&) =
      delete;
  ~PrintersExternalDataHandler() override;

  // CloudExternalDataPolicyObserver::Delegate:
  void OnExternalDataSet(const std::string& policy,
                         const std::string& user_id) override;
  void OnExternalDataCleared(const std::string& policy,
                             const std::string& user_id) override;
  void OnExternalDataFetched(const std::string& policy,
                             const std::string& user_id,
                             std::unique_ptr<std::string> data,
                             const base::FilePath& file_path) override;
  void RemoveForAccountId(const AccountId& account_id) override;

 private:
  const raw_ref<PrefService> local_state_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_PRINTERS_EXTERNAL_DATA_HANDLER_H_
