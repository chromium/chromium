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

  void SetList(base::Value::List ports) {
    local_state_.SetList(prefs::kExplicitlyAllowedNetworkPorts,
                         std::move(ports));
  }

  PrefService* local_state() { return &local_state_; }

 private:
  TestingPrefServiceSimple local_state_;
};

TEST_F(ConvertExplicitlyAllowedNetworkPortsPrefTest, EmptyList) {
  SetList(base::Value::List());
  auto ports = ConvertExplicitlyAllowedNetworkPortsPref(local_state());
  EXPECT_THAT(ports, IsEmpty());
}

TEST_F(ConvertExplicitlyAllowedNetworkPortsPrefTest, ValidList) {
  base::Value::List list;
  list.Append(20);
  list.Append(21);
  list.Append(22);
  SetList(std::move(list));
  auto ports = ConvertExplicitlyAllowedNetworkPortsPref(local_state());
  EXPECT_THAT(ports, ElementsAre(20, 21, 22));
}

// This shouldn't happen, but we handle it.
TEST_F(ConvertExplicitlyAllowedNetworkPortsPrefTest, ListOfBools) {
  base::Value::List list;
  list.Append(false);
  list.Append(true);
  SetList(std::move(list));
  auto ports = ConvertExplicitlyAllowedNetworkPortsPref(local_state());
  EXPECT_THAT(ports, IsEmpty());
}

// This really shouldn't happen.
TEST_F(ConvertExplicitlyAllowedNetworkPortsPrefTest, MixedTypesList) {
  base::Value::List list;
  list.Append(true);
  list.Append("79");
  list.Append(554);
  SetList(std::move(list));
  auto ports = ConvertExplicitlyAllowedNetworkPortsPref(local_state());
  EXPECT_THAT(ports, ElementsAre(554));
}

TEST_F(ConvertExplicitlyAllowedNetworkPortsPrefTest, OutOfRangeIntegers) {
  constexpr int kValues[] = {
      -1,      // Too small.
      100000,  // Too big.
      119,     // Valid.
  };
  base::Value::List list;
  for (const auto& value : kValues) {
    list.Append(value);
  }
  SetList(std::move(list));
  auto ports = ConvertExplicitlyAllowedNetworkPortsPref(local_state());
  EXPECT_THAT(ports, ElementsAre(119));
}
