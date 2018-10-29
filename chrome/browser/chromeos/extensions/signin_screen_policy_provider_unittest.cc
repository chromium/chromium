// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/extensions/signin_screen_policy_provider.h"
#include "components/version_info/version_info.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/features/feature_session_type.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

namespace {

const char kRandomExtensionId[] = "abacabadabacabaeabacabadabacabaf";
// Gnubby
const char kGnubbyExtensionId[] = "beknehfpfkghjoafdifaflglpjkojoco";
//  smart card connector
const char kSampleSigninExtensionId[] = "khpfeaanjngmcnplbdlpegiifgpfgdco";

scoped_refptr<const extensions::Extension> CreateTestApp(
    const std::string& extension_id,
    extensions::Manifest::Location location) {
  return extensions::ExtensionBuilder()
      .SetManifest(
          extensions::DictionaryBuilder()
              .Set("name", "test app")
              .Set("version", "1")
              .Set("manifest_version", 2)
              .Set("app",
                   extensions::DictionaryBuilder()
                       .Set("background",
                            extensions::DictionaryBuilder()
                                .Set("persistent", "false")
                                .Set("scripts", extensions::ListBuilder()
                                                    .Append("background.js")
                                                    .Build())
                                .Build())
                       .Build())
              .Set("storage",
                   extensions::DictionaryBuilder()
                       .Set("managed_schema", "managed_storage_schema.json")
                       .Build())
              .Set("permissions", extensions::ListBuilder()
                                      .Append("usb")
                                      .Append("alwaysOnTopWindows")
                                      .Build())
              .Build())
      .SetID(extension_id)
      .SetLocation(location)
      .Build();
}

}  // namespace

class SigninScreenPolicyProviderTest : public testing::Test {
 protected:
  chromeos::SigninScreenPolicyProvider provider_;
};

TEST_F(SigninScreenPolicyProviderTest, AllowPolicyExtensionOnDev) {
  // On dev channel every extension installed via policy should work.
  extensions::ScopedCurrentChannel channel(version_info::Channel::DEV);
  scoped_refptr<const extensions::Extension> extension = CreateTestApp(
      kRandomExtensionId, extensions::Manifest::Location::EXTERNAL_POLICY);
  base::string16 error;
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error));
  EXPECT_TRUE(error.empty());
}

TEST_F(SigninScreenPolicyProviderTest, DenyRandomPolicyExtensionOnStable) {
  // On stable channel arbitrary extension (though installed via policy)
  // should be blocked.
  extensions::ScopedCurrentChannel channel(version_info::Channel::STABLE);
  scoped_refptr<const extensions::Extension> extension = CreateTestApp(
      kRandomExtensionId, extensions::Manifest::Location::EXTERNAL_POLICY);
  base::string16 error;
  EXPECT_FALSE(provider_.UserMayLoad(extension.get(), &error));
  EXPECT_FALSE(error.empty());
}

TEST_F(SigninScreenPolicyProviderTest, AllowEssentialExtensionOnStable) {
  // Essential component extensions for the login screen should always work.
  extensions::ScopedCurrentChannel channel(version_info::Channel::STABLE);
  scoped_refptr<const extensions::Extension> extension = CreateTestApp(
      kGnubbyExtensionId, extensions::Manifest::Location::EXTERNAL_COMPONENT);
  base::string16 error;
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error));
  EXPECT_TRUE(error.empty());
}

TEST_F(SigninScreenPolicyProviderTest,
       AllowWhitelistedExtensionViaPolicyOnStable) {
  // Whitelisted Google-developed extensions should be available on
  // stable if installed via policy.
  // This test should be changed in future as we evolve feaature
  // requirements.
  extensions::ScopedCurrentChannel channel(version_info::Channel::STABLE);
  scoped_refptr<const extensions::Extension> extension =
      CreateTestApp(kSampleSigninExtensionId,
                    extensions::Manifest::Location::EXTERNAL_POLICY);
  base::string16 error;
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error));
  EXPECT_TRUE(error.empty());
}

TEST_F(SigninScreenPolicyProviderTest,
       DenyNonPolicyWhitelistedExtensionOnStable) {
  // Google-developed extensions, if not installed via policy, should
  // be disabled.
  extensions::ScopedCurrentChannel channel(version_info::Channel::STABLE);
  scoped_refptr<const extensions::Extension> extension =
      CreateTestApp(kSampleSigninExtensionId,
                    extensions::Manifest::Location::EXTERNAL_COMPONENT);
  base::string16 error;
  EXPECT_FALSE(provider_.UserMayLoad(extension.get(), &error));
  EXPECT_FALSE(error.empty());
}

TEST_F(SigninScreenPolicyProviderTest, DenyRandomNonPolicyExtensionOnDev) {
  extensions::ScopedCurrentChannel channel(version_info::Channel::DEV);
  scoped_refptr<const extensions::Extension> extension = CreateTestApp(
      kRandomExtensionId, extensions::Manifest::Location::EXTERNAL_COMPONENT);
  base::string16 error;
  EXPECT_FALSE(provider_.UserMayLoad(extension.get(), &error));
  EXPECT_FALSE(error.empty());
}
