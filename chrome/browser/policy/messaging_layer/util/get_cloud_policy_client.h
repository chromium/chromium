// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_GET_CLOUD_POLICY_CLIENT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_GET_CLOUD_POLICY_CLIENT_H_

#include "base/callback.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

using CloudPolicyClientResultCb =
    base::OnceCallback<void(StatusOr<policy::CloudPolicyClient*>)>;

using GetCloudPolicyClientCallback =
    base::RepeatingCallback<void(CloudPolicyClientResultCb)>;

// Returns the address of the function, |GetCloudPolicyClient|, which retrieves
// and operates on the |policy::CloudPolicyClient| object. See
// |GetCloudPolicyClient| for details.
GetCloudPolicyClientCallback GetCloudPolicyClientCb();

}  // namespace reporting
#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_GET_CLOUD_POLICY_CLIENT_H_
