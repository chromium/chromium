// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/backup/dict_pref_backup_serializer.h"

#include <string>
#include <vector>

#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dict_pref_backup_serializer {
namespace {

TEST(DictPrefBackupSerializerTest, ShouldDeserializeValidNonEmptyDict) {
  // Set up a dictionary pref with some arbitrary non-empty value.
  std::string pref_name = "dict";
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterDictionaryPref(pref_name);
  auto dict_value = base::Value::Dict().Set("key1", 1).Set("key2", "blah");
  pref_service.SetDict(pref_name, dict_value.Clone());

  // Serialize the dictionary, clear it from PrefService, then attempt to
  // deserialize and recover.
  std::string serialized_dict = GetSerializedDict(&pref_service, pref_name);
  pref_service.ClearPref(pref_name);
  SetDict(&pref_service, pref_name, serialized_dict);

  // The dictionary value should be the original one.
  EXPECT_EQ(pref_service.GetDict(pref_name), dict_value);
}

TEST(DictPrefBackupSerializerTest, ShouldDeserializeValidEmptyDict) {
  // Set up a dictionary pref in its default empty state.
  std::string pref_name = "dict";
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterDictionaryPref(pref_name);
  ASSERT_EQ(pref_service.GetDict(pref_name), base::Value::Dict());

  // Serialize the dictionary, change the pref, then attempt to deserialize and
  // recover.
  std::string serialized_dict = GetSerializedDict(&pref_service, pref_name);
  pref_service.SetDict(pref_name, base::Value::Dict().Set("key1", 1));
  SetDict(&pref_service, pref_name, serialized_dict);

  // The dictionary value should be the original one.
  EXPECT_EQ(pref_service.GetDict(pref_name), base::Value::Dict());
}

TEST(DictPrefBackupSerializerTest, ShouldNotDeserializeCorruptDict) {
  // Set up a dictionary pref with some arbitrary value.
  std::string pref_name = "dict";
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterDictionaryPref(pref_name);
  auto dict_value = base::Value::Dict().Set("key1", 1).Set("key2", "blah");
  pref_service.SetDict(pref_name, dict_value.Clone());

  SetDict(&pref_service, pref_name, "corrupted");

  // The dictionary value should be unchanged and the call should not crash.
  EXPECT_EQ(pref_service.GetDict(pref_name), dict_value);
}

}  // namespace
}  // namespace dict_pref_backup_serializer
