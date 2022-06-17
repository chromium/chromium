// Copyright 2022 The Chromium Authors. All rights reserved.
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
  auto container1 = GuestId{"test1", "test2"};
  auto container2 = GuestId{"test1", "test2"};
  auto container3 = GuestId{"test2", "test1"};

  ASSERT_TRUE(container1 == container2);
  ASSERT_FALSE(container1 == container3);
  ASSERT_FALSE(container2 == container3);
}

TEST_F(GuestIdTest, GuestIdFromDictValue) {
  base::Value dict(base::Value::Type::DICT);
  dict.SetStringKey(prefs::kVmKey, "foo");
  dict.SetStringKey(prefs::kContainerKey, "bar");
  EXPECT_TRUE(GuestId(dict) == GuestId("foo", "bar"));
}

TEST_F(GuestIdTest, GuestIdFromNonDictValue) {
  base::Value non_dict("not a dict value");
  EXPECT_TRUE(GuestId(non_dict) == GuestId("", ""));
}

TEST_F(GuestIdTest, DuplicateContainerNamesInPrefsAreRemoved) {
  GuestId container1("test1", "test1");
  base::Value::Dict dictionary1 = container1.ToDictValue();
  dictionary1.Set(prefs::kContainerOsPrettyNameKey, "Test OS Name 1");
  dictionary1.Set(prefs::kContainerOsVersionKey, 1);

  GuestId container2("test1", "test2");
  base::Value::Dict dictionary2 = container2.ToDictValue();
  dictionary2.Set(prefs::kContainerOsPrettyNameKey, "Test OS Name 2");
  dictionary2.Set(prefs::kContainerOsVersionKey, 2);

  GuestId container3("test2", "test1");
  base::Value::Dict dictionary3 = container3.ToDictValue();
  dictionary3.Set(prefs::kContainerOsPrettyNameKey, "Test OS Name 3");
  dictionary3.Set(prefs::kContainerOsVersionKey, 3);

  base::Value::List containers;
  containers.Append(dictionary1.Clone());
  containers.Append(dictionary2.Clone());
  containers.Append(dictionary1.Clone());
  containers.Append(dictionary2.Clone());
  containers.Append(dictionary3.Clone());

  PrefService* prefs = profile_.GetPrefs();
  prefs->SetList(prefs::kGuestOsContainers, std::move(containers));

  RemoveDuplicateContainerEntries(prefs);

  const base::Value::List& result =
      prefs->Get(prefs::kGuestOsContainers)->GetList();

  ASSERT_EQ(result.size(), 3);
  EXPECT_EQ(result[0].GetDict(), dictionary1);
  EXPECT_EQ(result[1].GetDict(), dictionary2);
  EXPECT_EQ(result[2].GetDict(), dictionary3);
}

TEST_F(GuestIdTest, GetContainers) {
  auto pref = base::JSONReader::Read(R"([
    {"vm_name": "vm1", "container_name": "c1"},
    {"vm_name": "vm2", "container_name": "c2"},
    {"vm_name": "vm3"}
  ])");
  ASSERT_TRUE(pref.has_value());
  profile_.GetPrefs()->Set(prefs::kGuestOsContainers, std::move(*pref));
  std::vector<GuestId> expected = {GuestId("vm1", "c1"), GuestId("vm2", "c2")};
  EXPECT_EQ(GetContainers(&profile_), expected);
}

}  // namespace guest_os
