// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/test/test_constants.h"

#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace enterprise::test {

const char kFakeCustomerId[] = "customer_id";
const char kDifferentCustomerId[] = "other_customer_id";

const char kTestUserId[] = "user_id";
const char kTestUserEmail[] = "test@example.com";

const char kProfileDmToken[] = "profile_dm_token";
const char kProfileClientId[] = "profile_client_id";

const char kBrowserDmToken[] = "browser_dm_token";
const char kBrowserClientId[] = "browser_client_id";

const char kDeviceDmToken[] = "device_dm_token";
const char kDeviceClientId[] = "device_client_id";

const char kEnrollmentToken[] = "enrollment_token";

policy::ClientStorage::ClientInfo CreateBrowserClientInfo() {
  policy::ClientStorage::ClientInfo client_info;
  client_info.device_id = kBrowserClientId;
  client_info.device_token = kBrowserDmToken;
  client_info.allowed_policy_types.insert(
      {policy::dm_protocol::kChromeMachineLevelUserCloudPolicyType,
       policy::dm_protocol::kChromeMachineLevelExtensionCloudPolicyType});
  return client_info;
}

}  // namespace enterprise::test
