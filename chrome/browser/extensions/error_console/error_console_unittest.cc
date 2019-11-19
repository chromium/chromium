// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/error_console/error_console.h"

#include <stddef.h>

#include <memory>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_error_test_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using error_test_util::CreateNewManifestError;
using error_test_util::CreateNewRuntimeError;

class ErrorConsoleUnitTest : public testing::Test {
 public:
  ErrorConsoleUnitTest() : error_console_(nullptr) {}
  ~ErrorConsoleUnitTest() override {}

  void SetUp() override {
    testing::Test::SetUp();
    // Errors are only kept if we have the FeatureSwitch and have Developer Mode
    // enabled.
    FeatureSwitch::error_console()->SetOverrideValue(
        FeatureSwitch::OVERRIDE_ENABLED);
    profile_.reset(new TestingProfile);
    profile_->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);
    error_console_ = ErrorConsole::Get(profile_.get());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  ErrorConsole* error_console_;
};

// Test that the error console is enabled/disabled appropriately.
TEST_F(ErrorConsoleUnitTest, EnableAndDisableErrorConsole) {
  profile_->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, false);

  // At the start, the error console should be disabled because the user is not
  // in developer mode.
  EXPECT_FALSE(error_console_->enabled());
  EXPECT_FALSE(error_console_->IsEnabledForChromeExtensionsPage());
  EXPECT_FALSE(error_console_->IsEnabledForAppsDeveloperTools());

  // Switch the error_console on.
  profile_->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);

  // The error console should now be enabled, and specifically enabled for the
  // chrome:extensions page.
  EXPECT_TRUE(error_console_->enabled());
  EXPECT_TRUE(error_console_->IsEnabledForChromeExtensionsPage());
  EXPECT_FALSE(error_console_->IsEnabledForAppsDeveloperTools());

  // If we disable developer mode, we should disable error console.
  profile_->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, false);
  EXPECT_FALSE(error_console_->enabled());
  EXPECT_FALSE(error_console_->IsEnabledForChromeExtensionsPage());
  EXPECT_FALSE(error_console_->IsEnabledForAppsDeveloperTools());

  // Installing the Chrome Apps and Extensions Developer Tools should enable
  // the ErrorConsole, same as if the profile were in developer mode.
  const char kAppsDeveloperToolsExtensionId[] =
      "ohmmkhmmmpcnpikjeljgnaoabkaalbgc";
  scoped_refptr<const Extension> adt =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "apps dev tools")
                           .Set("version", "0.2.0")
                           .Set("manifest_version", 2)
                           .Build())
          .SetID(kAppsDeveloperToolsExtensionId)
          .Build();
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_.get());
  registry->AddEnabled(adt);
  registry->TriggerOnLoaded(adt.get());
  EXPECT_TRUE(error_console_->enabled());
  EXPECT_FALSE(error_console_->IsEnabledForChromeExtensionsPage());
  EXPECT_TRUE(error_console_->IsEnabledForAppsDeveloperTools());

  // Unloading the Apps Developer Tools should disable error console.
  registry->RemoveEnabled(adt->id());
  registry->TriggerOnUnloaded(adt.get(), UnloadedExtensionReason::DISABLE);
  EXPECT_FALSE(error_console_->enabled());
  EXPECT_FALSE(error_console_->IsEnabledForChromeExtensionsPage());
  EXPECT_FALSE(error_console_->IsEnabledForAppsDeveloperTools());
}

// Test that the error console is enabled for all channels.
TEST_F(ErrorConsoleUnitTest, EnabledForAllChannels) {
  version_info::Channel channels[] = {
      version_info::Channel::UNKNOWN, version_info::Channel::CANARY,
      version_info::Channel::DEV, version_info::Channel::BETA,
      version_info::Channel::STABLE};
  for (const version_info::Channel channel : channels) {
    ScopedCurrentChannel channel_override(channel);
    ASSERT_EQ(channel, GetCurrentChannel());

    profile_->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, false);
    EXPECT_FALSE(error_console_->enabled());
    EXPECT_FALSE(error_console_->IsEnabledForChromeExtensionsPage());

    profile_->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);
    EXPECT_TRUE(error_console_->enabled());
    EXPECT_TRUE(error_console_->IsEnabledForChromeExtensionsPage());
  }
}

// Test that errors are successfully reported. This is a simple test, since it
// is tested more thoroughly in extensions/browser/error_map_unittest.cc
TEST_F(ErrorConsoleUnitTest, ReportErrors) {
  const size_t kNumTotalErrors = 6;
  const std::string kId = crx_file::id_util::GenerateId("id");
  error_console_->set_default_reporting_for_test(ExtensionError::MANIFEST_ERROR,
                                                 true);
  ASSERT_EQ(0u, error_console_->GetErrorsForExtension(kId).size());

  for (size_t i = 0; i < kNumTotalErrors; ++i) {
    error_console_->ReportError(
        CreateNewManifestError(kId, base::NumberToString(i)));
  }

  ASSERT_EQ(kNumTotalErrors, error_console_->GetErrorsForExtension(kId).size());
}

TEST_F(ErrorConsoleUnitTest, DontStoreErrorsWithoutEnablingType) {
  // Disable default runtime error reporting, and enable default manifest error
  // reporting.
  error_console_->set_default_reporting_for_test(ExtensionError::RUNTIME_ERROR,
                                                 false);
  error_console_->set_default_reporting_for_test(ExtensionError::MANIFEST_ERROR,
                                                 true);

  const std::string kId = crx_file::id_util::GenerateId("id");

  // Try to report a runtime error - it should be ignored.
  error_console_->ReportError(CreateNewRuntimeError(kId, "a"));
  ASSERT_EQ(0u, error_console_->GetErrorsForExtension(kId).size());

  // Check that manifest errors are reported successfully.
  error_console_->ReportError(CreateNewManifestError(kId, "b"));
  ASSERT_EQ(1u, error_console_->GetErrorsForExtension(kId).size());

  // We should still ignore runtime errors.
  error_console_->ReportError(CreateNewRuntimeError(kId, "c"));
  ASSERT_EQ(1u, error_console_->GetErrorsForExtension(kId).size());

  // Enable runtime errors specifically for this extension, and disable the use
  // of the default mask.
  error_console_->SetReportingForExtension(
      kId, ExtensionError::RUNTIME_ERROR, true);

  // We should now accept runtime and manifest errors.
  error_console_->ReportError(CreateNewManifestError(kId, "d"));
  ASSERT_EQ(2u, error_console_->GetErrorsForExtension(kId).size());
  error_console_->ReportError(CreateNewRuntimeError(kId, "e"));
  ASSERT_EQ(3u, error_console_->GetErrorsForExtension(kId).size());

  // All other extensions should still use the default mask, and ignore runtime
  // errors but report manifest errors.
  const std::string kId2 = crx_file::id_util::GenerateId("id2");
  error_console_->ReportError(CreateNewRuntimeError(kId2, "f"));
  ASSERT_EQ(0u, error_console_->GetErrorsForExtension(kId2).size());
  error_console_->ReportError(CreateNewManifestError(kId2, "g"));
  ASSERT_EQ(1u, error_console_->GetErrorsForExtension(kId2).size());

  // By reverting to default reporting, we should again allow manifest errors,
  // but not runtime errors.
  error_console_->UseDefaultReportingForExtension(kId);
  error_console_->ReportError(CreateNewManifestError(kId, "h"));
  ASSERT_EQ(4u, error_console_->GetErrorsForExtension(kId).size());
  error_console_->ReportError(CreateNewRuntimeError(kId, "i"));
  ASSERT_EQ(4u, error_console_->GetErrorsForExtension(kId).size());
}

// Test that we only store errors by default for unpacked extensions, and that
// assigning a preference to any extension overrides the defaults.
TEST_F(ErrorConsoleUnitTest, TestDefaultStoringPrefs) {
  // For this, we need actual extensions.
  scoped_refptr<const Extension> unpacked_extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "unpacked")
                           .Set("version", "0.0.1")
                           .Set("manifest_version", 2)
                           .Build())
          .SetLocation(Manifest::UNPACKED)
          .SetID(crx_file::id_util::GenerateId("unpacked"))
          .Build();
  scoped_refptr<const Extension> packed_extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "packed")
                           .Set("version", "0.0.1")
                           .Set("manifest_version", 2)
                           .Build())
          .SetLocation(Manifest::INTERNAL)
          .SetID(crx_file::id_util::GenerateId("packed"))
          .Build();

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_.get());
  registry->AddEnabled(unpacked_extension);
  registry->AddEnabled(packed_extension);

  // We should start with a clean slate.
  EXPECT_EQ(0u, error_console_->GetErrorsForExtension(
      unpacked_extension->id()).size());
  EXPECT_EQ(0u, error_console_->GetErrorsForExtension(
      packed_extension->id()).size());

  // Errors should be ignored by default for the packed extension.
  error_console_->ReportError(
      CreateNewManifestError(packed_extension->id(), "manifest error 1"));
  error_console_->ReportError(
      CreateNewRuntimeError(packed_extension->id(), "runtime error 1"));
  EXPECT_EQ(0u, error_console_->GetErrorsForExtension(
      packed_extension->id()).size());
  // Also check that reporting settings are correctly returned.
  EXPECT_FALSE(error_console_->IsReportingEnabledForExtension(
      packed_extension->id()));

  // Errors should be reported by default for the unpacked extension.
  error_console_->ReportError(
      CreateNewManifestError(unpacked_extension->id(), "manifest error 2"));
  error_console_->ReportError(
      CreateNewRuntimeError(unpacked_extension->id(), "runtime error 2"));
  EXPECT_EQ(2u, error_console_->GetErrorsForExtension(
      unpacked_extension->id()).size());
  // Also check that reporting settings are correctly returned.
  EXPECT_TRUE(error_console_->IsReportingEnabledForExtension(
      unpacked_extension->id()));

  // Registering a preference should override this for both types of extensions
  // (should be able to enable errors for packed, or disable errors for
  // unpacked).
  error_console_->SetReportingForExtension(packed_extension->id(),
                                           ExtensionError::RUNTIME_ERROR,
                                           true);
  error_console_->ReportError(
      CreateNewRuntimeError(packed_extension->id(), "runtime error 3"));
  EXPECT_EQ(1u, error_console_->GetErrorsForExtension(
      packed_extension->id()).size());
  EXPECT_TRUE(error_console_->IsReportingEnabledForExtension(
      packed_extension->id()));

  error_console_->SetReportingForExtension(unpacked_extension->id(),
                                           ExtensionError::RUNTIME_ERROR,
                                           false);
  error_console_->ReportError(
      CreateNewRuntimeError(packed_extension->id(), "runtime error 4"));
  EXPECT_EQ(2u,  // We should still have the first two errors.
            error_console_->GetErrorsForExtension(
                unpacked_extension->id()).size());
  EXPECT_FALSE(error_console_->IsReportingEnabledForExtension(
      unpacked_extension->id()));
}

}  // namespace extensions
