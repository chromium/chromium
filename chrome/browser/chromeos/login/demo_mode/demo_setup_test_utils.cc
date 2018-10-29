// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_setup_test_utils.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace chromeos {

namespace test {

bool SetupDummyOfflinePolicyDir(const std::string& account_id,
                                base::ScopedTempDir* temp_dir) {
  if (!temp_dir->CreateUniqueTempDir()) {
    LOG(ERROR) << "Failed to create unique tempdir";
    return false;
  }

  if (base::WriteFile(temp_dir->GetPath().AppendASCII("device_policy"), "",
                      0) != 0) {
    LOG(ERROR) << "Failed to create device_policy file";
    return false;
  }

  // We use MockCloudPolicyStore for the device local account policy in the
  // tests, thus actual policy content can be empty. account_id is specified
  // since it is used by DemoSetupController to look up the store.
  std::string policy_blob;
  if (!account_id.empty()) {
    enterprise_management::PolicyData policy_data;
    policy_data.set_username(account_id);
    enterprise_management::PolicyFetchResponse policy;
    policy.set_policy_data(policy_data.SerializeAsString());
    policy_blob = policy.SerializeAsString();
  }
  if (base::WriteFile(temp_dir->GetPath().AppendASCII("local_account_policy"),
                      policy_blob.data(), policy_blob.size()) !=
      static_cast<int>(policy_blob.size())) {
    LOG(ERROR) << "Failed to create local_account_policy file";
    return false;
  }
  return true;
}

}  // namespace test

}  // namespace chromeos
