// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/extensions_metrics_provider.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/test/task_environment.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/metrics/client_info.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test_enabled_state_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/extension_install.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"

using metrics::ExtensionInstallProto;
using extensions::Extension;
using extensions::ExtensionBuilder;
using extensions::Manifest;
using extensions::DictionaryBuilder;

namespace {

void StoreNoClientInfoBackup(const metrics::ClientInfo& /* client_info */) {
}

std::unique_ptr<metrics::ClientInfo> ReturnNoBackup() {
  return std::unique_ptr<metrics::ClientInfo>();
}

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
  std::unique_ptr<extensions::ExtensionSet> GetInstalledExtensions(
      Profile* profile) override {
    std::unique_ptr<extensions::ExtensionSet> extensions(
        new extensions::ExtensionSet());
    scoped_refptr<const extensions::Extension> extension;
    extension = extensions::ExtensionBuilder()
                    .SetManifest(extensions::DictionaryBuilder()
                                     .Set("name", "Test extension")
                                     .Set("version", "1.0.0")
                                     .Set("manifest_version", 2)
                                     .Build())
                    .SetID("ahfgeienlihckogmohjhadlkjgocpleb")
                    .Build();
    extensions->Insert(extension);
    extension = extensions::ExtensionBuilder()
                    .SetManifest(extensions::DictionaryBuilder()
                                     .Set("name", "Test extension 2")
                                     .Set("version", "1.0.0")
                                     .Set("manifest_version", 2)
                                     .Build())
                    .SetID("pknkgggnfecklokoggaggchhaebkajji")
                    .Build();
    extensions->Insert(extension);
    extension = extensions::ExtensionBuilder()
                    .SetManifest(extensions::DictionaryBuilder()
                                     .Set("name", "Colliding Extension")
                                     .Set("version", "1.0.0")
                                     .Set("manifest_version", 2)
                                     .Build())
                    .SetID("mdhofdjgenpkhlmddfaegdjddcecipmo")
                    .Build();
    extensions->Insert(extension);
    return extensions;
  }

  // Override GetClientID() to return a specific value on which test
  // expectations are based.
  uint64_t GetClientID() override { return 0x3f1bfee9; }
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

// Checks that the fake set of extensions provided by
// TestExtensionsMetricsProvider is encoded properly.
TEST(ExtensionsMetricsProvider, SystemProtoEncoding) {
  metrics::SystemProfileProto system_profile;
  base::test::TaskEnvironment task_environment;
  TestingProfileManager testing_profile_manager(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager.SetUp());
  TestingPrefServiceSimple local_state;
  metrics::TestEnabledStateProvider enabled_state_provider(true, true);
  metrics::MetricsService::RegisterPrefs(local_state.registry());
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager(
      metrics::MetricsStateManager::Create(
          &local_state, &enabled_state_provider, base::string16(),
          base::Bind(&StoreNoClientInfoBackup), base::Bind(&ReturnNoBackup)));
  TestExtensionsMetricsProvider extension_metrics(metrics_state_manager.get());
  extension_metrics.ProvideSystemProfileMetrics(&system_profile);
  ASSERT_EQ(2, system_profile.occupied_extension_bucket_size());
  EXPECT_EQ(10, system_profile.occupied_extension_bucket(0));
  EXPECT_EQ(1007, system_profile.occupied_extension_bucket(1));
}

class ExtensionMetricsProviderInstallsTest
    : public extensions::ExtensionServiceTestBase {
 public:
  ExtensionMetricsProviderInstallsTest() {}
  ~ExtensionMetricsProviderInstallsTest() override {}

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
    prefs_ = extensions::ExtensionPrefs::Get(profile());

    last_sample_time_ = base::Time::Now() - base::TimeDelta::FromMinutes(30);
  }

  ExtensionInstallProto ConstructProto(const Extension& extension) {
    return ExtensionsMetricsProvider::ConstructInstallProtoForTesting(
        extension, prefs_, last_sample_time_);
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
  extensions::ExtensionPrefs* prefs_ = nullptr;
  base::Time last_sample_time_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMetricsProviderInstallsTest);
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
        ExtensionBuilder("test").SetLocation(Manifest::INTERNAL).Build();
    add_extension(extension.get());
    ExtensionInstallProto install = ConstructProto(*extension);
    EXPECT_TRUE(install.has_type());
    EXPECT_EQ(ExtensionInstallProto::EXTENSION, install.type());

    EXPECT_TRUE(install.has_install_location());
    EXPECT_EQ(ExtensionInstallProto::INTERNAL, install.install_location());

    EXPECT_TRUE(install.has_manifest_version());
    EXPECT_EQ(2, install.manifest_version());

    EXPECT_TRUE(install.has_action_type());
    EXPECT_EQ(ExtensionInstallProto::NO_ACTION, install.action_type());

    EXPECT_TRUE(install.has_has_file_access());
    EXPECT_FALSE(install.has_file_access());

    EXPECT_TRUE(install.has_has_incognito_access());
    EXPECT_FALSE(install.has_incognito_access());

    EXPECT_TRUE(install.has_updates_from_store());
    EXPECT_FALSE(install.updates_from_store());

    EXPECT_TRUE(install.has_is_from_bookmark());
    EXPECT_FALSE(install.is_from_bookmark());

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
            .SetLocation(Manifest::INTERNAL)
            .Build();
    add_extension(extension.get());
    ExtensionInstallProto install = ConstructProto(*extension);
    EXPECT_EQ(ExtensionInstallProto::PLATFORM_APP, install.type());
  }

  {
    // Test the install location.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("unpacked").SetLocation(Manifest::UNPACKED).Build();
    add_extension(extension.get());
    ExtensionInstallProto install = ConstructProto(*extension);
    EXPECT_EQ(ExtensionInstallProto::UNPACKED, install.install_location());
  }

  {
    // Test the extension action as a browser action.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("browser_action")
            .SetLocation(Manifest::INTERNAL)
            .SetAction(ExtensionBuilder::ActionType::BROWSER_ACTION)
            .Build();
    add_extension(extension.get());
    ExtensionInstallProto install = ConstructProto(*extension);
    EXPECT_EQ(ExtensionInstallProto::BROWSER_ACTION, install.action_type());
  }

  {
    // Test the extension action as a page action.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("page_action")
            .SetLocation(Manifest::INTERNAL)
            .SetAction(ExtensionBuilder::ActionType::PAGE_ACTION)
            .Build();
    add_extension(extension.get());
    ExtensionInstallProto install = ConstructProto(*extension);
    EXPECT_EQ(ExtensionInstallProto::PAGE_ACTION, install.action_type());
  }

  {
    // Test the disable reasons field.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("disable_reasons")
            .SetLocation(Manifest::INTERNAL)
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
    DictionaryBuilder background;
    background.Set("persistent", false)
        .Set("scripts", extensions::ListBuilder().Append("script.js").Build());
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("event_page")
            .SetLocation(Manifest::INTERNAL)
            .MergeManifest(DictionaryBuilder()
                               .Set("background", background.Build())
                               .Build())
            .Build();
    add_extension(extension.get());
    ExtensionInstallProto install = ConstructProto(*extension);
    EXPECT_EQ(ExtensionInstallProto::EVENT_PAGE,
              install.background_script_type());
  }

  {
    // Test that persistent background pages are reported correctly.
    DictionaryBuilder background;
    background.Set("persistent", true)
        .Set("scripts", extensions::ListBuilder().Append("script.js").Build());
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("persisent_background")
            .SetLocation(Manifest::INTERNAL)
            .MergeManifest(DictionaryBuilder()
                               .Set("background", background.Build())
                               .Build())
            .Build();
    add_extension(extension.get());
    ExtensionInstallProto install = ConstructProto(*extension);
    EXPECT_EQ(ExtensionInstallProto::PERSISTENT_BACKGROUND_PAGE,
              install.background_script_type());
  }

  {
    // Test changing the blacklist state.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("blacklist").SetLocation(Manifest::INTERNAL).Build();
    add_extension(extension.get());
    prefs()->SetExtensionBlacklistState(
        extension->id(), extensions::BLACKLISTED_SECURITY_VULNERABILITY);
    ExtensionInstallProto install = ConstructProto(*extension);
    EXPECT_EQ(ExtensionInstallProto::BLACKLISTED_SECURITY_VULNERABILITY,
              install.blacklist_state());
  }

  {
    // Test that the installed_in_this_sample_period boolean is correctly
    // reported.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("installtime").SetLocation(Manifest::INTERNAL).Build();
    add_extension(extension.get());
    set_last_sample_time(base::Time::Now() + base::TimeDelta::FromMinutes(60));
    ExtensionInstallProto install = ConstructProto(*extension);
    EXPECT_FALSE(install.installed_in_this_sample_period());
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
  EXPECT_TRUE(std::any_of(installs.begin(), installs.end(),
                          [](const ExtensionInstallProto& install) {
                            return install.type() ==
                                   ExtensionInstallProto::EXTENSION;
                          }));
  EXPECT_TRUE(std::any_of(installs.begin(), installs.end(),
                          [](const ExtensionInstallProto& install) {
                            return install.type() ==
                                   ExtensionInstallProto::PLATFORM_APP;
                          }));
}
