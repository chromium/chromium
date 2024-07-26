// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_RESTRICTION_SCHEDULE_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_RESTRICTION_SCHEDULE_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefRegistrySimple;

namespace policy {

class DeviceRestrictionSchedulePolicyHandler {
 public:
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_RESTRICTION_SCHEDULE_POLICY_HANDLER_H_
