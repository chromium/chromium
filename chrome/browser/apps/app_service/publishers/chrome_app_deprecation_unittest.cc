// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/chrome_app_deprecation.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/common/pref_names.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/test/test_extension_dir.h"

using extensions::ChromeTestExtensionLoader;
using extensions::CrxInstaller;
using extensions::Extension;
using extensions::ExtensionBuilder;
using extensions::ExtensionRegistry;
using extensions::TestExtensionDir;
using extensions::mojom::ManifestLocation;

namespace apps::chrome_app_deprecation {

constexpr std::string_view kHistogram =
    "Apps.AppLaunch.ChromeAppsDeprecationCheck";

class ChromeAppDeprecationTest : public extensions::ExtensionServiceTestBase {
 protected:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();

    InitializeEmptyExtensionService();

    app_ = InstallTestApp(profile());
    ASSERT_TRUE(app_);
    ASSERT_TRUE(registrar()->IsExtensionEnabled(app_->id()));
  }

  void TearDown() override {
    app_.release()->Release();

    ExtensionServiceTestBase::TearDown();
  }

  scoped_refptr<const Extension> InstallTestApp(Profile* profile) {
    // Build a simple Chrome App.
    base::Value::Dict manifest =
        base::Value::Dict()
            .Set("name", "Test app")
            .Set("version", "1.0.0")
            .Set("manifest_version", 3)
            .Set("description", "an extension")
            .Set("app", base::Value::Dict().Set(
                            "launch", base::Value::Dict().Set("local_path",
                                                              "test.html")));

    TestExtensionDir good_extension_dir;
    good_extension_dir.WriteManifest(manifest);

    ChromeTestExtensionLoader loader(profile);
    loader.set_pack_extension(false);
    return loader.LoadExtension(good_extension_dir.UnpackedPath());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<const Extension> app_;

  base::HistogramTester histogram_tester_;
};

TEST_F(ChromeAppDeprecationTest, DefaultFeatureFlag) {
  scoped_feature_list_.InitWithEmptyFeatureAndFieldTrialLists();
  ASSERT_TRUE(base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchAllowed);

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kHistogram),
      BucketsAre(base::Bucket(
          /*DeprecationCheckOutcome::kUserInstalledAllowedByFlag*/ 0, 1)));
}

TEST_F(ChromeAppDeprecationTest, DefaultFeatureFlagNotChromeApp) {
  scoped_feature_list_.InitWithEmptyFeatureAndFieldTrialLists();
  ASSERT_TRUE(base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps));

  EXPECT_EQ(HandleDeprecation("Not a Chrome App id", profile()),
            DeprecationStatus::kLaunchAllowed);

  EXPECT_THAT(histogram_tester_.GetAllSamples(kHistogram),
              BucketsAre(base::Bucket(
                  /*DeprecationCheckOutcome::kAllowedNotChromeApp*/ 11, 1)));
}

TEST_F(ChromeAppDeprecationTest, DisabledFeatureFlag) {
  scoped_feature_list_.InitAndDisableFeature(kAllowUserInstalledChromeApps);
  ASSERT_FALSE(base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchBlocked);

  EXPECT_THAT(histogram_tester_.GetAllSamples(kHistogram),
              BucketsAre(base::Bucket(
                  /*DeprecationCheckOutcome::kUserInstalledBlocked*/ 2, 1)));
}

TEST_F(ChromeAppDeprecationTest, DisabledFeatureFlagNotChromeApp) {
  scoped_feature_list_.InitAndDisableFeature(kAllowUserInstalledChromeApps);
  ASSERT_FALSE(base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps));

  EXPECT_EQ(HandleDeprecation("Not a Chrome App id", profile()),
            DeprecationStatus::kLaunchAllowed);

  EXPECT_THAT(histogram_tester_.GetAllSamples(kHistogram),
              BucketsAre(base::Bucket(
                  /*DeprecationCheckOutcome::kAllowedNotChromeApp*/ 11, 1)));
}

TEST_F(ChromeAppDeprecationTest, EnabledFeatureFlag) {
  scoped_feature_list_.InitAndEnableFeature(kAllowUserInstalledChromeApps);
  ASSERT_TRUE(base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchAllowed);

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kHistogram),
      BucketsAre(base::Bucket(
          /*DeprecationCheckOutcome::kUserInstalledAllowedByFlag*/ 0, 1)));
}

TEST_F(ChromeAppDeprecationTest, EnabledFeatureFlagNotChromeApp) {
  scoped_feature_list_.InitAndEnableFeature(kAllowUserInstalledChromeApps);
  ASSERT_TRUE(base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps));

  EXPECT_EQ(HandleDeprecation("Not a Chrome App id", profile()),
            DeprecationStatus::kLaunchAllowed);

  EXPECT_THAT(histogram_tester_.GetAllSamples(kHistogram),
              BucketsAre(base::Bucket(
                  /*DeprecationCheckOutcome::kAllowedNotChromeApp*/ 11, 1)));
}

class ChromeAppDeprecationKioskTest : public ChromeAppDeprecationTest {
 protected:
  void SetUp() override {
    ChromeAppDeprecationTest::SetUp();

    SetKioskSessionForTesting();
  }

  void TearDown() override {
    SetKioskSessionForTesting(false);

    ChromeAppDeprecationTest::TearDown();
  }
};

TEST_F(ChromeAppDeprecationKioskTest, DefaultFeatureFlag) {
  scoped_feature_list_.InitWithEmptyFeatureAndFieldTrialLists();
  ASSERT_FALSE(base::FeatureList::IsEnabled(kAllowChromeAppsInKioskSessions));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchBlocked);

  EXPECT_THAT(histogram_tester_.GetAllSamples(kHistogram),
              BucketsAre(base::Bucket(
                  /*DeprecationCheckOutcome::kKioskModeBlocked*/ 6, 1)));
}

TEST_F(ChromeAppDeprecationKioskTest, DisabledFeatureFlag) {
  scoped_feature_list_.InitAndDisableFeature(kAllowChromeAppsInKioskSessions);
  ASSERT_FALSE(base::FeatureList::IsEnabled(kAllowChromeAppsInKioskSessions));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchBlocked);

  EXPECT_THAT(histogram_tester_.GetAllSamples(kHistogram),
              BucketsAre(base::Bucket(
                  /*DeprecationCheckOutcome::kKioskModeBlocked*/ 6, 1)));
}

TEST_F(ChromeAppDeprecationKioskTest, EnabledFeatureFlag) {
  scoped_feature_list_.InitAndEnableFeature(kAllowChromeAppsInKioskSessions);
  ASSERT_TRUE(base::FeatureList::IsEnabled(kAllowChromeAppsInKioskSessions));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchAllowed);

  EXPECT_THAT(histogram_tester_.GetAllSamples(kHistogram),
              BucketsAre(base::Bucket(
                  /*DeprecationCheckOutcome::kKioskModeAllowedByFlag*/ 3, 1)));
}

TEST_F(ChromeAppDeprecationKioskTest, DisabledFeatureFlagDefaultPolicy) {
  scoped_feature_list_.InitAndDisableFeature(kAllowChromeAppsInKioskSessions);
  ASSERT_FALSE(base::FeatureList::IsEnabled(kAllowChromeAppsInKioskSessions));
  ASSERT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kKioskChromeAppsForceAllowed));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchBlocked);

  EXPECT_THAT(histogram_tester_.GetAllSamples(kHistogram),
              BucketsAre(base::Bucket(
                  /*DeprecationCheckOutcome::kKioskModeBlocked*/ 6, 1)));
}

TEST_F(ChromeAppDeprecationKioskTest, DisabledFeatureFlagOverridenByPolicy) {
  scoped_feature_list_.InitAndDisableFeature(kAllowChromeAppsInKioskSessions);
  ASSERT_FALSE(base::FeatureList::IsEnabled(kAllowChromeAppsInKioskSessions));

  profile()->GetPrefs()->SetBoolean(prefs::kKioskChromeAppsForceAllowed, true);
  ASSERT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kKioskChromeAppsForceAllowed));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchAllowed);

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kHistogram),
      BucketsAre(base::Bucket(
          /*DeprecationCheckOutcome::kKioskModeAllowedByAdminPolicy*/ 5, 1)));
}

class ChromeAppDeprecationUserInstalledAllowlistTest
    : public ChromeAppDeprecationTest {
 protected:
  void SetUp() override {
    ChromeAppDeprecationTest::SetUp();

    AddAppToAllowlistForTesting(app_->id());
  }
};

TEST_F(ChromeAppDeprecationUserInstalledAllowlistTest, DefaultFeatureFlag) {
  scoped_feature_list_.InitWithEmptyFeatureAndFieldTrialLists();
  ASSERT_TRUE(base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchAllowed);

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kHistogram),
      BucketsAre(base::Bucket(
          /*DeprecationCheckOutcome::kUserInstalledAllowedByAllowlist*/ 1, 1)));
}

TEST_F(ChromeAppDeprecationUserInstalledAllowlistTest, DisabledFeatureFlag) {
  scoped_feature_list_.InitAndDisableFeature(kAllowUserInstalledChromeApps);
  ASSERT_FALSE(base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchAllowed);

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kHistogram),
      BucketsAre(base::Bucket(
          /*DeprecationCheckOutcome::kUserInstalledAllowedByAllowlist*/ 1, 1)));
}

TEST_F(ChromeAppDeprecationUserInstalledAllowlistTest, EnabledFeatureFlag) {
  scoped_feature_list_.InitAndEnableFeature(kAllowUserInstalledChromeApps);
  ASSERT_TRUE(base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchAllowed);

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kHistogram),
      BucketsAre(base::Bucket(
          /*DeprecationCheckOutcome::kUserInstalledAllowedByAllowlist*/ 1, 1)));
}

class ChromeAppDeprecationKioskAllowlistTest
    : public ChromeAppDeprecationKioskTest {
 protected:
  void SetUp() override {
    ChromeAppDeprecationKioskTest::SetUp();

    AddAppToAllowlistForTesting(app_->id());
  }
};

TEST_F(ChromeAppDeprecationKioskAllowlistTest, DefaultFeatureFlag) {
  scoped_feature_list_.InitWithEmptyFeatureAndFieldTrialLists();
  ASSERT_TRUE(base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchAllowed);

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kHistogram),
      BucketsAre(base::Bucket(
          /*DeprecationCheckOutcome::kKioskModeAllowedByAllowlist*/ 4, 1)));
}

TEST_F(ChromeAppDeprecationKioskAllowlistTest, DisabledFeatureFlag) {
  scoped_feature_list_.InitAndDisableFeature(kAllowUserInstalledChromeApps);
  ASSERT_FALSE(base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchAllowed);

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kHistogram),
      BucketsAre(base::Bucket(
          /*DeprecationCheckOutcome::kKioskModeAllowedByAllowlist*/ 4, 1)));
}

TEST_F(ChromeAppDeprecationKioskAllowlistTest, EnabledFeatureFlag) {
  scoped_feature_list_.InitAndEnableFeature(kAllowUserInstalledChromeApps);
  ASSERT_TRUE(base::FeatureList::IsEnabled(kAllowUserInstalledChromeApps));

  EXPECT_EQ(HandleDeprecation(app_->id(), profile()),
            DeprecationStatus::kLaunchAllowed);

  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kHistogram),
      BucketsAre(base::Bucket(
          /*DeprecationCheckOutcome::kKioskModeAllowedByAllowlist*/ 4, 1)));
}

class ChromeAppDeprecationComponentUpdaterAllowlistTest
    : public ChromeAppDeprecationTest {
 protected:
  void SetUp() override {
    ChromeAppDeprecationTest::SetUp();

    // Disable all deprecation feature flags.
    scoped_feature_list_.InitWithFeatures(
        {}, {kAllowUserInstalledChromeApps, kAllowChromeAppsInKioskSessions});
  }

  void TearDown() override {
    // This remains set across multiple tests.
    SetKioskSessionForTesting(false);

    ChromeAppDeprecationTest::TearDown();
  }
};

TEST_F(ChromeAppDeprecationComponentUpdaterAllowlistTest, LoadCommonAllowlist) {
  ChromeAppDeprecation::DynamicAllowlists allowlists;
  allowlists.mutable_common_allowlist()->Add(std::string(app_->id()));

  AssignComponentUpdaterAllowlistsForTesting(base::Version("1.0.0"),
                                             allowlists);

  EXPECT_EQ(DeprecationStatus::kLaunchAllowed,
            HandleDeprecation(app_->id(), profile()));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kHistogram),
      BucketsAre(base::Bucket(
          /*DeprecationCheckOutcome::kUserInstalledAllowedByAllowlist*/ 1, 1)));

  SetKioskSessionForTesting();
  EXPECT_EQ(DeprecationStatus::kLaunchAllowed,
            HandleDeprecation(app_->id(), profile()));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kHistogram),
      BucketsAre(
          base::Bucket(
              /*DeprecationCheckOutcome::kUserInstalledAllowedByAllowlist*/ 1,
              1),
          base::Bucket(
              /*DeprecationCheckOutcome::kKioskModeAllowedByAllowlist*/ 4, 1)));
}

TEST_F(ChromeAppDeprecationComponentUpdaterAllowlistTest,
       LoadUserInstalledAllowlist) {
  ChromeAppDeprecation::DynamicAllowlists allowlists;
  allowlists.mutable_user_installed_allowlist()->Add(std::string(app_->id()));

  AssignComponentUpdaterAllowlistsForTesting(base::Version("1.0.0"),
                                             allowlists);

  EXPECT_EQ(DeprecationStatus::kLaunchAllowed,
            HandleDeprecation(app_->id(), profile()));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kHistogram),
      BucketsAre(base::Bucket(
          /*DeprecationCheckOutcome::kUserInstalledAllowedByAllowlist*/ 1, 1)));

  SetKioskSessionForTesting();
  EXPECT_EQ(DeprecationStatus::kLaunchBlocked,
            HandleDeprecation(app_->id(), profile()));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kHistogram),
      BucketsAre(
          base::Bucket(
              /*DeprecationCheckOutcome::kUserInstalledAllowedByAllowlist*/ 1,
              1),
          base::Bucket(
              /*DeprecationCheckOutcome::kKioskModeBlocked*/ 6, 1)));
}

TEST_F(ChromeAppDeprecationComponentUpdaterAllowlistTest, LoadKioskAllowlist) {
  ChromeAppDeprecation::DynamicAllowlists allowlists;
  allowlists.mutable_kiosk_session_allowlist()->Add(std::string(app_->id()));

  AssignComponentUpdaterAllowlistsForTesting(base::Version("1.0.0"),
                                             allowlists);

  EXPECT_EQ(DeprecationStatus::kLaunchBlocked,
            HandleDeprecation(app_->id(), profile()));
  EXPECT_THAT(histogram_tester_.GetAllSamples(kHistogram),
              BucketsAre(base::Bucket(
                  /*DeprecationCheckOutcome::kUserInstalledBlocked*/ 2, 1)));

  SetKioskSessionForTesting();
  EXPECT_EQ(DeprecationStatus::kLaunchAllowed,
            HandleDeprecation(app_->id(), profile()));
  EXPECT_THAT(
      histogram_tester_.GetAllSamples(kHistogram),
      BucketsAre(
          base::Bucket(
              /*DeprecationCheckOutcome::kUserInstalledBlocked*/ 2, 1),
          base::Bucket(
              /*DeprecationCheckOutcome::kKioskModeAllowedByAllowlist*/ 4, 1)));
}

}  // namespace apps::chrome_app_deprecation
