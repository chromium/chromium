// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_POLICY_INVALIDATION_UTIL_H_
#define CHROME_BROWSER_POLICY_CLOUD_POLICY_INVALIDATION_UTIL_H_

#include "components/invalidation/public/invalidation_util.h"

namespace enterprise_management {

class PolicyData;

}  // namespace enterprise_management

namespace invalidation {

class ObjectId;

}  // namespace invalidation

namespace policy {

// Returns true if |topic| is a public topic. If |topic| is public,
// publish/subscribe service will fan out all the outgoing messages to all the
// devices subscribed to this topic.
// For example:
// If device subscribes to "DeviceGuestModeEnabled" public topic all the
// instances subscribed to this topic will receive all the outgoing messages
// addressed to topic "DeviceGuestModeEnabled". But if 2 devices with different
// InstanceID subscribe to private topic "BOOKMARK", they will receive different
// set of messages addressed to pair ("BOOKMARK", InstanceID) respectievely.
bool IsPublicInvalidationTopic(const syncer::Topic& topic);

// Returns true if |policy| has data about policy to invalidate, and saves
// that data in |object_id|, and false otherwise.
bool GetCloudPolicyObjectIdFromPolicy(
    const enterprise_management::PolicyData& policy,
    invalidation::ObjectId* object_id);

// The same as GetCloudPolicyObjectIdFromPolicy but gets the |object_id| for
// remote command.
bool GetRemoteCommandObjectIdFromPolicy(
    const enterprise_management::PolicyData& policy,
    invalidation::ObjectId* object_id);

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_POLICY_INVALIDATION_UTIL_H_
