// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_settings.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class GoogleUpdateTest : public PlatformTest {
 protected:
  GoogleUpdateTest() : user_data_dir_override_(chrome::DIR_USER_DATA) {}
  ~GoogleUpdateTest() override {}

 private:
  base::ScopedPathOverride user_data_dir_override_;

  DISALLOW_COPY_AND_ASSIGN(GoogleUpdateTest);
};

TEST_F(GoogleUpdateTest, StatsConsent) {
  // Stats are off by default.
  EXPECT_FALSE(GoogleUpdateSettings::GetCollectStatsConsent());
  // Stats reporting is ON.
  EXPECT_TRUE(GoogleUpdateSettings::SetCollectStatsConsent(true));
  EXPECT_TRUE(GoogleUpdateSettings::GetCollectStatsConsent());
  // Stats reporting is OFF.
  EXPECT_TRUE(GoogleUpdateSettings::SetCollectStatsConsent(false));
  EXPECT_FALSE(GoogleUpdateSettings::GetCollectStatsConsent());
}

#if defined(OS_WIN)

TEST_F(GoogleUpdateTest, LastRunTime) {
  // Querying the value that does not exists should fail.
  EXPECT_TRUE(GoogleUpdateSettings::RemoveLastRunTime());
  EXPECT_EQ(-1, GoogleUpdateSettings::GetLastRunTime());
  // Setting and querying the last update time in fast sequence
  // should give 0 days.
  EXPECT_TRUE(GoogleUpdateSettings::SetLastRunTime());
  EXPECT_EQ(0, GoogleUpdateSettings::GetLastRunTime());
}

#endif  // defined(OS_WIN)

TEST_F(GoogleUpdateTest, IsOrganic) {
  // Test some brand codes to ensure that future changes to this method won't
  // go unnoticed.

  // GGRV is non-organic.
  EXPECT_FALSE(google_brand::IsOrganic("GGRV"));

  // Other GGR* are organic.
  EXPECT_TRUE(google_brand::IsOrganic("GGRA"));

  // GGLS must always be organic.
  EXPECT_TRUE(google_brand::IsOrganic("GGLS"));
}

TEST_F(GoogleUpdateTest, IsOrganicFirstRunBrandCodes) {
  // Test some brand codes to ensure that future changes to this method won't
  // go unnoticed.
  EXPECT_FALSE(google_brand::IsOrganicFirstRun("CHFO"));
  EXPECT_FALSE(google_brand::IsOrganicFirstRun("CHMA"));
  EXPECT_TRUE(google_brand::IsOrganicFirstRun("EUBA"));
  EXPECT_TRUE(google_brand::IsOrganicFirstRun("GGRA"));

#if defined(OS_MAC)
  // An empty brand string on Mac is used for channels other than stable,
  // which are always organic.
  EXPECT_TRUE(google_brand::IsOrganicFirstRun(""));
#endif
}

TEST_F(GoogleUpdateTest, IsEnterpriseBrandCodes) {
  EXPECT_TRUE(google_brand::IsEnterprise("GGRV"));
  std::string gce_prefix = "GCE";
  for (char ch = 'A'; ch <= 'Z'; ++ch)
    EXPECT_TRUE(google_brand::IsEnterprise(gce_prefix + ch));
  EXPECT_FALSE(google_brand::IsEnterprise("ggrv"));
  EXPECT_FALSE(google_brand::IsEnterprise("gcea"));
  EXPECT_FALSE(google_brand::IsEnterprise("GGRA"));
  EXPECT_FALSE(google_brand::IsEnterprise("AGCE"));
  EXPECT_FALSE(google_brand::IsEnterprise("GCCE"));
  EXPECT_FALSE(google_brand::IsEnterprise("CHFO"));
  EXPECT_FALSE(google_brand::IsEnterprise("CHMA"));
  EXPECT_FALSE(google_brand::IsEnterprise("EUBA"));
  EXPECT_FALSE(google_brand::IsEnterprise(""));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Test for http://crbug.com/383003
TEST_F(GoogleUpdateTest, ConsentFileIsWorldReadable) {
  // Turn on stats reporting.
  EXPECT_TRUE(GoogleUpdateSettings::SetCollectStatsConsent(true));

  base::FilePath consent_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &consent_dir));
  ASSERT_TRUE(base::DirectoryExists(consent_dir));

  base::FilePath consent_file = consent_dir.Append("Consent To Send Stats");
  ASSERT_TRUE(base::PathExists(consent_file));
  int permissions;
  ASSERT_TRUE(base::GetPosixFilePermissions(consent_file, &permissions));
  EXPECT_TRUE(permissions & base::FILE_PERMISSION_READ_BY_OTHERS);
}
#endif
