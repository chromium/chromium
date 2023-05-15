// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_id.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace guest_os {

class GuestIdTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(GuestIdTest, GuestIdEquality) {
  auto container1 = GuestId{VmType::TERMINA, "test1", "test2"};
  auto container2 = GuestId{VmType::TERMINA, "test1", "test2"};
  auto container3 = GuestId{VmType::BOREALIS, "test2", "test1"};
  auto container4 = GuestId{VmType::BOREALIS, "test1", "test2"};

  ASSERT_TRUE(container1 == container2);
  ASSERT_TRUE(container1 == container4);  // Type is ignored for comparisons
  ASSERT_FALSE(container1 == container3);
  ASSERT_FALSE(container2 == container3);
}

TEST_F(GuestIdTest, GuestIdFromDictValue) {
  {
    auto dict = base::Value::Dict()
                    .Set(prefs::kVmNameKey, "foo")
                    .Set(prefs::kContainerNameKey, "bar");
    EXPECT_TRUE(GuestId(base::Value(std::move(dict))) ==
                GuestId(VmType::TERMINA, "foo", "bar"));
  }

  {
    auto dict = base::Value::Dict()
                    .Set(prefs::kVmTypeKey, 0)
                    .Set(prefs::kVmNameKey, "foo")
                    .Set(prefs::kContainerNameKey, "bar");
    EXPECT_TRUE(GuestId(base::Value(std::move(dict))) ==
                GuestId(VmType::TERMINA, "foo", "bar"));
  }

  {
    auto dict = base::Value::Dict()
                    .Set(prefs::kVmTypeKey, 1)
                    .Set(prefs::kVmNameKey, "foo")
                    .Set(prefs::kContainerNameKey, "bar");
    EXPECT_TRUE(GuestId(base::Value(std::move(dict))) ==
                GuestId(VmType::PLUGIN_VM, "foo", "bar"));
  }
}

TEST_F(GuestIdTest, GuestIdFromNonDictValue) {
  base::Value non_dict("not a dict value");
  EXPECT_TRUE(GuestId(non_dict) == GuestId(VmType::UNKNOWN, "", ""));
}

TEST_F(GuestIdTest, GetContainers) {
  auto pref = base::JSONReader::Read(R"([
    {"vm_name": "vm1", "container_name": "c1"},
    {"vm_name": "vm2", "container_name": "c2"},
    {"vm_name": "vm3"}
  ])");
  ASSERT_TRUE(pref.has_value());
  profile_.GetPrefs()->Set(prefs::kGuestOsContainers, std::move(*pref));
  std::vector<GuestId> expected = {GuestId(VmType::TERMINA, "vm1", "c1"),
                                   GuestId(VmType::TERMINA, "vm2", "c2"),
                                   GuestId(VmType::TERMINA, "vm3", "")};
  EXPECT_EQ(GetContainers(&profile_, VmType::TERMINA), expected);
}

TEST_F(GuestIdTest, VmTypeFromPref) {
  EXPECT_EQ(VmType::UNKNOWN, VmTypeFromPref(base::Value("not-dict")));
  base::Value dict(base::Value::Type::DICT);
  EXPECT_EQ(VmType::TERMINA, VmTypeFromPref(dict));
  dict.GetDict().Set("vm_type", 1);
  EXPECT_EQ(VmType::PLUGIN_VM, VmTypeFromPref(dict));
  dict.GetDict().Set("vm_type", 999);
  EXPECT_EQ(VmType::UNKNOWN, VmTypeFromPref(dict));
}

TEST_F(GuestIdTest, RoundTripViaPrefs) {
  auto id = guest_os::GuestId(guest_os::VmType::PLUGIN_VM, "vm_name",
                              "container_name");
  AddContainerToPrefs(&profile_, id, {});
  auto list = GetContainers(&profile_, VmType::PLUGIN_VM);
  ASSERT_EQ(list.size(), 1u);
  EXPECT_EQ(list[0], id);
}

}  // namespace guest_os
