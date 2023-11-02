// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_POLICY_REMOVER_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_POLICY_REMOVER_H_

#include <vector>

#include "components/policy/proto/chrome_device_policy.pb.h"

namespace policy {

// Remove |policy_proto_tags_to_remove| fields from |policies|.
// Vector |policy_proto_tags_to_remove| contains device policy proto tags
// from ChromeDeviceSettingsProto. Implementation of this method is generated
// automatically by generate_device_policy_remover.py.
void RemovePolicies(enterprise_management::ChromeDeviceSettingsProto* policies,
                    const std::vector<int>& policy_proto_tags_to_remove);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_POLICY_REMOVER_H_
