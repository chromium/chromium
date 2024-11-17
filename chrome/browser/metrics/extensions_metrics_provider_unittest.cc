// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/extensions_metrics_provider.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/extension_install.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

using extensions::Extension;
using extensions::ExtensionBuilder;
using extensions::Manifest;
using extensions::mojom::ManifestLocation;
using metrics::ExtensionInstallProto;

namespace {

constexpr char kTestUserEmail[] = "user@example.com";

class TestExtensionsMetricsProvider : public ExtensionsMetricsProvider {
 public:
  explicit TestExtensionsMetricsProvider(
      metrics::MetricsStateManager* metrics_state_manager)
      : ExtensionsMetricsProvider(metrics_state_manager) {}

  // Makes the protected HashExtension method available to testing code.
  using ExtensionsMetricsProvider::HashExtension;

 protected:
  // Override the GetInstalledExtensions method to return a set of extensions
  // for tests.
  std::optional<extensions::ExtensionSet> GetInstalledExtensions(
      Profile* profile) override {
    extensions::ExtensionSet extensions;
    extensions.Insert(extensions::ExtensionBuilder()
                          .SetManifest(base::Value::Dict()
                                           .Set("name", "Test extension")
                                           .Set("version", "1.0.0")
                                           .Set("manifest_version", 2))
                          .SetID("ahfgeienlihckogmohjhadlkjgocpleb")
                          .Build());
    extensions.Insert(extensions::ExtensionBuilder()
                          .SetManifest(base::Value::Dict()
                                           .Set("name", "Test extension 2")
                                           .Set("version", "1.0.0")
                                           .Set("manifest_version", 2))
                          .SetID("pknkgggnfecklokoggaggchhaebkajji")
                          .Build());
    extensions.Insert(extensions::ExtensionBuilder()
                          .SetManifest(base::Value::Dict()
                                           .Set("name", "Colliding Extension")
                                           .Set("version", "1.0.0")
                                           .Set("manifest_version", 2))
                          .SetID("mdhofdjgenpkhlmddfaegdjddcecipmo")
                          .Build());
    return extensions;
  }

  // Override GetClientID() to return a specific value on which test
  // expectations are based.
  uint64_t GetClientID() const override { return 0x3f1bfee9; }
};

}  // namespace

// Checks that the hash function used to hide precise extension IDs produces
// the expected values.
TEST(ExtensionsMetricsProvider, HashExtension) {
  EXPECT_EQ(978,
            TestExtensionsMetricsProvider::HashExtension(
                "ahfgeienlihckogmohjhadlkjgocpleb", 0));
  EXPECT_EQ(10,
            TestExtensionsMetricsProvider::HashExtension(
                "ahfgeienlihckogmohjhadlkjgocpleb", 3817));
  EXPECT_EQ(1007,
            TestExtensionsMetricsProvider::HashExtension(
                "pknkgggnfecklokoggaggchhaebkajji", 3817));
  EXPECT_EQ(10,
            TestExtensionsMetricsProvider::HashExtension(
                "mdhofdjgenpkhlmddfaegdjddcecipmo", 3817));
}

class ExtensionsMetricsProviderTest : public testing::Test {
 public:
  ExtensionsMetricsProviderTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        enabled_state_provider_(/*consent=*/true,
                                /*enabled=*/true) {}

  void SetUp() override {
    testing::Test::SetUp();
    EXPECT_TRUE(profile_manager_.SetUp());

#if BUILDFLAG(IS_CHROMEOS_ASH)
    auto* fake_user_manager = new ash::FakeChromeUserManager();
    scoped_user_manager_enabler_ =
        std::make_unique<user_manager::ScopedUserManager>(
            base::WrapUnique(fake_user_manager));
    const AccountId account_id(AccountId::FromUserEmail(kTestUserEmail));
    fake_user_manager->AddUser(account_id);
    fake_user_manager->LoginUser(account_id);
#endif

    metrics::MetricsService::RegisterPrefs(prefs_.registry());
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &prefs_, &enabled_state_provider_, std::wstring(), base::FilePath());
    metrics_state_manager_->InstantiateFieldTrialList();
  }

  void TearDown() override { profile_manager_.DeleteAllTestingProfiles(); }

  Profile* CreateTestingProfile(const std::string& test_email) {
    Profile* profile = profile_manager_.CreateTestingProfile(
        test_email, /* is_main_profile= */ true);
    profiles::SetLastUsedProfile(profile->GetBaseName());
    return profile;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple prefs_;
  TestingProfileManager profile_manager_;
  base::HistogramTester histogram_tester_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_enabler_;
#endif

  metrics::TestEnabledStateProvider enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
};

TEST_F(ExtensionsMetricsProviderTest, SystemProtoEncoding) {
  metrics::SystemProfileProto system_profile;

  TestExtensionsMetricsProvider extension_metrics_provider(
      metrics_state_manager_.get());
  extension_metrics_provider.ProvideSystemProfileMetrics(&system_profile);

  ASSERT_EQ(2, system_profile.occupied_extension_bucket_size());
  EXPECT_EQ(10, system_profile.occupied_extension_bucket(0));
  EXPECT_EQ(1007, system_profile.occupied_extension_bucket(1));
}

TEST_F(ExtensionsMetricsProviderTest, ProvideCurrentSessionData) {
  metrics::ChromeUserMetricsExtension uma_proto;
  Profile* profile = CreateTestingProfile(kTestUserEmail);
  TestExtensionsMetricsProvider extension_metrics_provider(
      metrics_state_manager_.get());

  // Set developer mode to OFF and verify false is recorded.
  extensions::util::SetDeveloperModeForProfile(profile, false);
  extension_metrics_provider.ProvideCurrentSessionData(&uma_proto);

  histogram_tester_.ExpectBucketCount("Extensions.DeveloperModeStatusEnabled",
                                      false, 1);

  // Set developer mode to ON and verify true is recorded.
  extensions::util::SetDeveloperModeForProfile(profile, true);
  extension_metrics_provider.ProvideCurrentSessionData(&uma_proto);

  histogram_tester_.ExpectBucketCount("Extensions.DeveloperModeStatusEnabled",
                                      true, 1);
  histogram_tester_.ExpectTotalCount("Extensions.DeveloperModeStatusEnabled",
                                     2);
}

class ExtensionMetricsProviderInstallsTest
    : public extensions::ExtensionServiceTestBase {
 public:
  ExtensionMetricsProviderInstallsTest() {}

  ExtensionMetricsProviderInstallsTest(
      const ExtensionMetricsProviderInstallsTest&) = delete;
  ExtensionMetricsProviderInstallsTest& operator=(
      const ExtensionMetricsProviderInstallsTest&) = delete;

  ~ExtensionMetricsProviderInstallsTest() override {}

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
    prefs_ = extensions::ExtensionPrefs::Get(profile());

    last_sample_time_ = base::Time::Now() - base::Minutes(30);
  }

  ExtensionInstallProto ConstructProto(const Extension& extension) {
    return ExtensionsMetricsProvider::ConstructInstallProtoForTesting(
        extension, prefs_, last_sample_time_, profile());
  }
  std::vector<ExtensionInstallProto> GetInstallsForProfile() {
    return ExtensionsMetricsProvider::GetInstallsForProfileForTesting(
        profile(), last_sample_time_);
  }

  extensions::ExtensionPrefs* prefs() { return prefs_; }
  void set_last_sample_time(base::Time last_sample_time) {
    last_sample_time_ = last_sample_time;
  }

 private:
  raw_ptr<extensions::ExtensionPrefs> prefs_ = nullptr;
  base::Time last_sample_time_;
};

// Tests the various aspects of constructing a relevant proto for a given
// extension installation.
TEST_F(ExtensionMetricsProviderInstallsTest, TestProtoConstruction) {
  auto add_extension = [this](const Extension* extension) {
    prefs()->OnExtensionInstalled(extension, Extension::ENABLED,
                                  syncer::StringOrdinal(), std::string());
  };

  {
    // Test basic prototype construction. All fields should be present, except
    // disable reasons (which should be empty).
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("test")
            .SetLocation(ManifestLocation::kInternal)
            .Build();
    add_extension(extension.get());
    ExtensionInstallProto install = ConstructProto(*extension);
    EXPECT_TRUE(install.has_type());
    EXPECT_EQ(ExtensionInstallProto::EXTENSION, install.type());

    EXPECT_TRUE(install.has_install_location());
    EXPECT_EQ(ExtensionInstallProto::INTERNAL, install.install_location());

    EXPECT_TRUE(install.has_manifest_version());
    EXPECT_EQ(3, install.manifest_version());

    EXPECT_TRUE(install.has_action_type());
    EXPECT_EQ(ExtensionInstallProto::NO_ACTION, install.action_type());

    EXPECT_TRUE(install.has_has_file_access());
    EXPECT_FALSE(install.has_file_access());

    EXPECT_TRUE(install.has_has_incognito_access());
    EXPECT_FALSE(install.has_incognito_access());

    EXPECT_TRUE(install.has_updates_from_store());
    EXPECT_FALSE(install.updates_from_store());

    EXPECT_TRUE(install.has_is_converted_from_user_script());
    EXPECT_FALSE(install.is_converted_from_user_script());

    EXPECT_TRUE(install.has_is_default_installed());
    EXPECT_FALSE(install.is_default_installed());

    EXPECT_TRUE(install.has_is_oem_installed());
    EXPECT_FALSE(install.is_oem_installed());

    EXPECT_TRUE(install.has_background_script_type());
    EXPECT_EQ(ExtensionInstallProto::NO_BACKGROUND_SCRIPT,
              install.background_script_type());

    EXPECT_EQ(0, install.disable_reasons_size());

    EXPECT_TRUE(install.has_blacklist_state());
    EXPECT_EQ(ExtensionInstallProto::NOT_BLACKLISTED,
              install.blacklist_state());

    EXPECT_TRUE(install.has_installed_in_this_sample_period());
    EXPECT_TRUE(install.installed_in_this_sample_period());

    EXPECT_TRUE(install.has_in_extensions_developer_mode());
    EXPECT_FALSE(install.in_extensions_developer_mode());
  }

  // It's not helpful to exhaustively test each possible variation of each
  // field in the proto (since in many cases the test code would then be
  // re-writing the original code), but we test a few of the more interesting
  // cases.

  {
    // Test the type() field; extensions of different types should be reported
    // as such.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("app", ExtensionBuilder::Type::PLATFORM_APP)
            .SetLocation(ManifestLocation::kInternal)
            .Build();
    add_extension(extension.get());
    ExtensionInstallProto install = ConstructProto(*extension);
    EXPECT_EQ(ExtensionInstallProto::PLATFORM_APP, install.type());
  }

  {
    // Test the install location.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("unpacked")
            .SetLocation(ManifestLocation::kUnpacked)
            .Build();
    add_extension(extension.get());
    ExtensionInstallProto install = ConstructProto(*extension);
    EXPECT_EQ(ExtensionInstallProto::UNPACKED, install.install_location());
  }

  {
    // Test the extension action as a browser action.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("browser_action")
            .SetLocation(ManifestLocation::kInternal)
            .SetManifestVersion(2)
            .SetAction(extensions::ActionInfo::Type::kBrowser)
            .Build();
    add_extension(extension.get());
    ExtensionInstallProto install = ConstructProto(*extension);
    EXPECT_EQ(ExtensionInstallProto::BROWSER_ACTION, install.action_type());
  }

  {
    // Test the extension action as a page action.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("page_action")
            .SetLocation(ManifestLocation::kInternal)
            .SetManifestVersion(2)
            .SetAction(extensions::ActionInfo::Type::kPage)
            .Build();
    add_extension(extension.get());
    ExtensionInstallProto install = ConstructProto(*extension);
    EXPECT_EQ(ExtensionInstallProto::PAGE_ACTION, install.action_type());
  }

  {
    // Test the disable reasons field.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("disable_reasons")
            .SetLocation(ManifestLocation::kInternal)
            .Build();
    add_extension(extension.get());
    prefs()->SetExtensionDisabled(
        extension->id(), extensions::disable_reason::DISABLE_USER_ACTION);
    {
      ExtensionInstallProto install = ConstructProto(*extension);
      ASSERT_EQ(1, install.disable_reasons_size());
      EXPECT_EQ(ExtensionInstallProto::USER_ACTION,
                install.disable_reasons().Get(0));
    }
    // Adding additional disable reasons should result in all reasons being
    // reported.
    prefs()->AddDisableReason(extension->id(),
                              extensions::disable_reason::DISABLE_CORRUPTED);
    {
      ExtensionInstallProto install = ConstructProto(*extension);
      ASSERT_EQ(2, install.disable_reasons_size());
      EXPECT_EQ(ExtensionInstallProto::USER_ACTION,
                install.disable_reasons().Get(0));
      EXPECT_EQ(ExtensionInstallProto::CORRUPTED,
                install.disable_reasons().Get(1));
    }
  }

  {
    // Test that event pages are reported correctly.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("event_page")
            .SetLocation(ManifestLocation::kInternal)
            .SetManifestVersion(2)
            .SetBackgroundContext(
                ExtensionBuilder::BackgroundContext::EVENT_PAGE)
            .Build();
    add_extension(extension.get());
    ExtensionInstallProto install = ConstructProto(*extension);
    EXPECT_EQ(ExtensionInstallProto::EVENT_PAGE,
              install.background_script_type());
  }

  {
    // Test that persistent background pages are reported correctly.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("persisent_background")
            .SetLocation(ManifestLocation::kInternal)
            .SetManifestVersion(2)
            .SetBackgroundContext(
                ExtensionBuilder::BackgroundContext::BACKGROUND_PAGE)
            .Build();
    add_extension(extension.get());
    ExtensionInstallProto install = ConstructProto(*extension);
    EXPECT_EQ(ExtensionInstallProto::PERSISTENT_BACKGROUND_PAGE,
              install.background_script_type());
  }
  {
    // Test that service worker scripts are reported correctly.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("service worker")
            .SetBackgroundContext(
                ExtensionBuilder::BackgroundContext::SERVICE_WORKER)
            .Build();
    add_extension(extension.get());
    ExtensionInstallProto install = ConstructProto(*extension);
    EXPECT_EQ(ExtensionInstallProto::SERVICE_WORKER,
              install.background_script_type());
  }

  {
    // Test changing the blacklist state.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("blacklist")
            .SetLocation(ManifestLocation::kInternal)
            .Build();
    add_extension(extension.get());
    extensions::blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
        extension->id(),
        extensions::BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY,
        prefs());
    ExtensionInstallProto install = ConstructProto(*extension);
    EXPECT_EQ(ExtensionInstallProto::BLACKLISTED_SECURITY_VULNERABILITY,
              install.blacklist_state());
  }

  {
    // Test that the installed_in_this_sample_period boolean is correctly
    // reported.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("installtime")
            .SetLocation(ManifestLocation::kInternal)
            .Build();
    add_extension(extension.get());
    set_last_sample_time(base::Time::Now() + base::Minutes(60));
    ExtensionInstallProto install = ConstructProto(*extension);
    EXPECT_FALSE(install.installed_in_this_sample_period());
  }

  {
    // Test that the `in_extensions_developer_mode` boolean is correctly
    // reported when developer mode is ON.
    extensions::util::SetDeveloperModeForProfile(profile(), true);
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("test")
            .SetLocation(ManifestLocation::kInternal)
            .Build();
    add_extension(extension.get());

    ExtensionInstallProto install = ConstructProto(*extension);

    EXPECT_TRUE(install.has_in_extensions_developer_mode());
    EXPECT_TRUE(install.in_extensions_developer_mode());
  }
}

// Tests that we retrieve all extensions associated with a given profile.
TEST_F(ExtensionMetricsProviderInstallsTest,
       TestGettingAllExtensionsInProfile) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").Build();
  service()->AddExtension(extension.get());
  scoped_refptr<const Extension> app =
      ExtensionBuilder("app", ExtensionBuilder::Type::PLATFORM_APP).Build();
  service()->AddExtension(app.get());
  service()->DisableExtension(app->id(),
                              extensions::disable_reason::DISABLE_USER_ACTION);

  std::vector<ExtensionInstallProto> installs = GetInstallsForProfile();
  // There should be two installs total.
  ASSERT_EQ(2u, installs.size());
  // One should be the extension, and the other should be the app. We don't
  // check the specifics of the proto, since that's tested above.
  EXPECT_TRUE(base::Contains(installs, ExtensionInstallProto::EXTENSION,
                             &ExtensionInstallProto::type));
  EXPECT_TRUE(base::Contains(installs, ExtensionInstallProto::PLATFORM_APP,
                             &ExtensionInstallProto::type));
}
