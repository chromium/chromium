// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/convert_explicitly_allowed_network_ports_pref.h"

#include <memory>
#include <string>

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::IsEmpty;

class ConvertExplicitlyAllowedNetworkPortsPrefTest : public ::testing::Test {
 public:
  ConvertExplicitlyAllowedNetworkPortsPrefTest() {
    local_state_.registry()->RegisterListPref(
        prefs::kExplicitlyAllowedNetworkPorts);
  }

  void SetManagedPref(base::Value ports) {
    local_state_.SetManagedPref(
        prefs::kExplicitlyAllowedNetworkPorts,
        base::Value::ToUniquePtrValue(std::move(ports)));
  }

  PrefService* local_state() { return &local_state_; }

 private:
  TestingPrefServiceSimple local_state_;
};

TEST_F(ConvertExplicitlyAllowedNetworkPortsPrefTest, EmptyList) {
  SetManagedPref(base::Value(base::Value::Type::LIST));
  auto ports = ConvertExplicitlyAllowedNetworkPortsPref(local_state());
  EXPECT_THAT(ports, IsEmpty());
}

TEST_F(ConvertExplicitlyAllowedNetworkPortsPrefTest, ValidList) {
  base::Value list(base::Value::Type::LIST);
  list.Append(base::Value(20));
  list.Append(base::Value(21));
  list.Append(base::Value(22));
  SetManagedPref(std::move(list));
  auto ports = ConvertExplicitlyAllowedNetworkPortsPref(local_state());
  EXPECT_THAT(ports, ElementsAre(20, 21, 22));
}

// This shouldn't happen, but we handle it.
TEST_F(ConvertExplicitlyAllowedNetworkPortsPrefTest, ListOfBools) {
  base::Value list(base::Value::Type::LIST);
  list.Append(base::Value(false));
  list.Append(base::Value(true));
  SetManagedPref(std::move(list));
  auto ports = ConvertExplicitlyAllowedNetworkPortsPref(local_state());
  EXPECT_THAT(ports, IsEmpty());
}

// This really shouldn't happen.
TEST_F(ConvertExplicitlyAllowedNetworkPortsPrefTest, MixedTypesList) {
  base::Value list(base::Value::Type::LIST);
  list.Append(base::Value(true));
  list.Append(base::Value("79"));
  list.Append(base::Value(554));
  SetManagedPref(std::move(list));
  auto ports = ConvertExplicitlyAllowedNetworkPortsPref(local_state());
  EXPECT_THAT(ports, ElementsAre(554));
}

TEST_F(ConvertExplicitlyAllowedNetworkPortsPrefTest, OutOfRangeIntegers) {
  constexpr int kValues[] = {
      -1,      // Too small.
      100000,  // Too big.
      119,     // Valid.
  };
  base::Value list(base::Value::Type::LIST);
  for (const auto& value : kValues) {
    list.Append(base::Value(value));
  }
  SetManagedPref(std::move(list));
  auto ports = ConvertExplicitlyAllowedNetworkPortsPref(local_state());
  EXPECT_THAT(ports, ElementsAre(119));
}
