// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_storage_id_salt.h"

#include "chrome/common/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(MediaStorageIdSalt, Register) {
  TestingPrefServiceSimple prefs;

  MediaStorageIdSalt::RegisterProfilePrefs(prefs.registry());
}

TEST(MediaStorageIdSalt, Create) {
  TestingPrefServiceSimple prefs;

  MediaStorageIdSalt::RegisterProfilePrefs(prefs.registry());
  std::vector<uint8_t> salt = MediaStorageIdSalt::GetSalt(&prefs);
  EXPECT_EQ(static_cast<size_t>(MediaStorageIdSalt::kSaltLength), salt.size());
}

TEST(MediaStorageIdSalt, Recreate) {
  TestingPrefServiceSimple prefs;

  MediaStorageIdSalt::RegisterProfilePrefs(prefs.registry());
  std::vector<uint8_t> original_salt = MediaStorageIdSalt::GetSalt(&prefs);
  EXPECT_EQ(static_cast<size_t>(MediaStorageIdSalt::kSaltLength),
            original_salt.size());

  // Now that the salt is created, mess it up and then try fetching it again
  // (should generate a new salt and log an error).
  prefs.SetString(prefs::kMediaStorageIdSalt, "123");
  std::vector<uint8_t> new_salt = MediaStorageIdSalt::GetSalt(&prefs);
  EXPECT_EQ(static_cast<size_t>(MediaStorageIdSalt::kSaltLength),
            new_salt.size());
  EXPECT_NE(original_salt, new_salt);
}

TEST(MediaStorageIdSalt, FetchTwice) {
  TestingPrefServiceSimple prefs;

  MediaStorageIdSalt::RegisterProfilePrefs(prefs.registry());
  std::vector<uint8_t> salt1 = MediaStorageIdSalt::GetSalt(&prefs);
  EXPECT_EQ(static_cast<size_t>(MediaStorageIdSalt::kSaltLength), salt1.size());

  // Fetch the salt again. Should be the same value.
  std::vector<uint8_t> salt2 = MediaStorageIdSalt::GetSalt(&prefs);
  EXPECT_EQ(static_cast<size_t>(MediaStorageIdSalt::kSaltLength), salt2.size());
  EXPECT_EQ(salt1, salt2);
}
