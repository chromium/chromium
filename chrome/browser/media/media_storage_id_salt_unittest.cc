// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_storage_id_salt.h"

#include <string>
#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MediaStorageIdSaltTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(MediaStorageIdSaltTest, Register) {
  TestingPrefServiceSimple prefs;

  MediaStorageIdSalt::RegisterProfilePrefs(prefs.registry());
}

TEST_F(MediaStorageIdSaltTest, Create) {
  std::vector<uint8_t> salt = MediaStorageIdSalt::GetSalt(&profile_);
  EXPECT_EQ(static_cast<size_t>(MediaStorageIdSalt::kSaltLength), salt.size());
}

TEST_F(MediaStorageIdSaltTest, Recreate) {
  std::vector<uint8_t> original_salt = MediaStorageIdSalt::GetSalt(&profile_);
  EXPECT_EQ(static_cast<size_t>(MediaStorageIdSalt::kSaltLength),
            original_salt.size());

  // Now that the salt is created, mess it up and then try fetching it again
  // (should generate a new salt and log an error).
  profile_.GetPrefs()->SetString(prefs::kMediaStorageIdSalt, "123");
  std::vector<uint8_t> new_salt = MediaStorageIdSalt::GetSalt(&profile_);
  EXPECT_EQ(static_cast<size_t>(MediaStorageIdSalt::kSaltLength),
            new_salt.size());
  EXPECT_NE(original_salt, new_salt);
}

TEST_F(MediaStorageIdSaltTest, FetchTwice) {
  std::vector<uint8_t> salt1 = MediaStorageIdSalt::GetSalt(&profile_);
  EXPECT_EQ(static_cast<size_t>(MediaStorageIdSalt::kSaltLength), salt1.size());

  // Fetch the salt again. Should be the same value.
  std::vector<uint8_t> salt2 = MediaStorageIdSalt::GetSalt(&profile_);
  EXPECT_EQ(static_cast<size_t>(MediaStorageIdSalt::kSaltLength), salt2.size());
  EXPECT_EQ(salt1, salt2);
}

TEST_F(MediaStorageIdSaltTest, OffTheRecord) {
  std::vector<uint8_t> regular_salt = MediaStorageIdSalt::GetSalt(&profile_);
  EXPECT_EQ(static_cast<size_t>(MediaStorageIdSalt::kSaltLength),
            regular_salt.size());

  TestingProfile* incognito_profile =
      TestingProfile::Builder().BuildIncognito(&profile_);
  std::vector<uint8_t> incognito_salt1 =
      MediaStorageIdSalt::GetSalt(incognito_profile);
  EXPECT_EQ(static_cast<size_t>(MediaStorageIdSalt::kSaltLength),
            incognito_salt1.size());
  EXPECT_NE(regular_salt, incognito_salt1);

  // Calling again on the same Incognito profile should return the same salt.
  std::vector<uint8_t> incognito_salt2 =
      MediaStorageIdSalt::GetSalt(incognito_profile);
  EXPECT_EQ(incognito_salt1, incognito_salt2);
}

TEST_F(MediaStorageIdSaltTest, IncognitoFirstDoesNotLeakToRegular) {
  TestingProfile* incognito_profile =
      TestingProfile::Builder().BuildIncognito(&profile_);
  std::vector<uint8_t> incognito_salt =
      MediaStorageIdSalt::GetSalt(incognito_profile);
  EXPECT_EQ(static_cast<size_t>(MediaStorageIdSalt::kSaltLength),
            incognito_salt.size());

  // Fetching the regular profile's salt after Incognito should generate a
  // distinct salt and not reuse or leak the Incognito salt.
  std::vector<uint8_t> regular_salt = MediaStorageIdSalt::GetSalt(&profile_);
  EXPECT_EQ(static_cast<size_t>(MediaStorageIdSalt::kSaltLength),
            regular_salt.size());
  EXPECT_NE(incognito_salt, regular_salt);
}
