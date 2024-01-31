// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/parent_access_code/parent_access_test_utils.h"

#include <memory>
#include <vector>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace parent_access {

// Dictionary keys for ParentAccessCodeConfig policy.
constexpr char kFutureConfigDictKey[] = "future_config";
constexpr char kCurrentConfigDictKey[] = "current_config";
constexpr char kOldConfigsDictKey[] = "old_configs";

AccessCodeConfig GetDefaultTestConfig() {
  return AccessCodeConfig(kTestSharedSecret, kDefaultCodeValidity,
                          kDefaultClockDrift);
}

AccessCodeConfig GetInvalidTestConfig() {
  return AccessCodeConfig("AAAAaaaaBBBBbbbbccccCCCC", kDefaultCodeValidity,
                          kDefaultClockDrift);
}

void GetTestAccessCodeValues(AccessCodeValues* test_values) {
  base::Time timestamp;
  ASSERT_TRUE(base::Time::FromString("8 Jan 2019 16:58:07 PST", &timestamp));
  (*test_values)[timestamp] = "734261";
  ASSERT_TRUE(base::Time::FromString("14 Jan 2019 15:35:05 PST", &timestamp));
  (*test_values)[timestamp] = "472150";
  ASSERT_TRUE(base::Time::FromString("14 Jan 2019 15:42:49 PST", &timestamp));
  (*test_values)[timestamp] = "204984";
  ASSERT_TRUE(base::Time::FromString("14 Jan 2019 15:53:01 PST", &timestamp));
  (*test_values)[timestamp] = "157758";
  ASSERT_TRUE(base::Time::FromString("14 Jan 2019 16:00:00 PST", &timestamp));
  (*test_values)[timestamp] = "524186";
}

base::Value PolicyFromConfigs(
    const AccessCodeConfig& future_config,
    const AccessCodeConfig& current_config,
    const std::vector<AccessCodeConfig>& old_configs) {
  base::Value::Dict dict;
  dict.Set(kFutureConfigDictKey, future_config.ToDictionary());
  dict.Set(kCurrentConfigDictKey, current_config.ToDictionary());
  base::Value::List old_configs_value;
  for (const auto& config : old_configs) {
    old_configs_value.Append(config.ToDictionary());
  }
  dict.Set(kOldConfigsDictKey, std::move(old_configs_value));
  return base::Value(std::move(dict));
}

}  // namespace parent_access
}  // namespace ash
