// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/installer/util/google_update_settings.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if BUILDFLAG(IS_WIN)
#include "base/test/test_reg_util_win.h"
#endif  // BUILDFLAG(IS_WIN)

class GoogleUpdateTest : public PlatformTest {
 public:
  GoogleUpdateTest(const GoogleUpdateTest&) = delete;
  GoogleUpdateTest& operator=(const GoogleUpdateTest&) = delete;

#if BUILDFLAG(IS_WIN)
  void SetUp() override {
    // Override HKCU to prevent writing to real keys. On Windows, the metrics
    // reporting consent is stored in the registry, and it is used to determine
    // the metrics reporting state when it is unset (e.g. during tests, which
    // start with fresh user data dirs). Otherwise, this may cause flakiness
    // since tests will sometimes start with metrics reporting enabled and
    // sometimes disabled.
    ASSERT_NO_FATAL_FAILURE(
        override_manager_.OverrideRegistry(HKEY_CURRENT_USER));

    PlatformTest::SetUp();
  }
#endif  // BUILDFLAG(IS_WIN)

 protected:
  GoogleUpdateTest() : user_data_dir_override_(chrome::DIR_USER_DATA) {}
  ~GoogleUpdateTest() override = default;

 private:
#if BUILDFLAG(IS_WIN)
  registry_util::RegistryOverrideManager override_manager_;
#endif  // BUILDFLAG(IS_WIN)

  base::ScopedPathOverride user_data_dir_override_;
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

#if BUILDFLAG(IS_WIN)

TEST_F(GoogleUpdateTest, LastRunTime) {
  // Querying the value that does not exists should fail.
  EXPECT_TRUE(GoogleUpdateSettings::RemoveLastRunTime());
  EXPECT_EQ(-1, GoogleUpdateSettings::GetLastRunTime());
  // Setting and querying the last update time in fast sequence
  // should give 0 days.
  EXPECT_TRUE(GoogleUpdateSettings::SetLastRunTime());
  EXPECT_EQ(0, GoogleUpdateSettings::GetLastRunTime());
}

#endif  // BUILDFLAG(IS_WIN)

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

#if BUILDFLAG(IS_MAC)
  // An empty brand string on Mac is used for channels other than stable,
  // which are always organic.
  EXPECT_TRUE(google_brand::IsOrganicFirstRun(""));
#endif
}

TEST_F(GoogleUpdateTest, IsEnterpriseBrandCodes) {
  EXPECT_TRUE(google_brand::IsEnterprise("GGRV"));
  std::string gce_prefix = "GCE";
  for (char ch = 'A'; ch <= 'Z'; ++ch) {
    EXPECT_EQ(google_brand::IsEnterprise(gce_prefix + ch), ch != 'L');
  }
  for (const std::string prefix :
       {"GCC", "GCF", "GCG", "GCH", "GCK", "GCL", "GCM", "GCO", "GCP", "GCQ",
        "GCS", "GCT", "GCU", "GCV", "GCW"}) {
    for (char ch = 'A'; ch <= 'Z'; ++ch) {
      EXPECT_TRUE(google_brand::IsEnterprise(prefix + ch));
    }
  }
  EXPECT_FALSE(google_brand::IsEnterprise("ggrv"));
  EXPECT_FALSE(google_brand::IsEnterprise("gcea"));
  EXPECT_FALSE(google_brand::IsEnterprise("GGRA"));
  EXPECT_FALSE(google_brand::IsEnterprise("AGCE"));
  EXPECT_FALSE(google_brand::IsEnterprise("GCZE"));
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
