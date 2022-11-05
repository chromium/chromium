// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/signin_screen_policy_provider.h"

#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/version_info/version_info.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;
using extensions::mojom::ManifestLocation;

namespace {

const char kRandomExtensionId[] = "abacabadabacabaeabacabadabacabaf";
//  smart card connector
const char kSampleSigninExtensionId[] = "khpfeaanjngmcnplbdlpegiifgpfgdco";

scoped_refptr<const extensions::Extension> CreateTestApp(
    const std::string& extension_id,
    ManifestLocation location) {
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

TEST_F(SigninScreenPolicyProviderTest, DenyRandomPolicyExtension) {
  // Arbitrary extension (though installed via policy) should be blocked.
  scoped_refptr<const extensions::Extension> extension =
      CreateTestApp(kRandomExtensionId, ManifestLocation::kExternalPolicy);
  std::u16string error;
  EXPECT_FALSE(provider_.UserMayLoad(extension.get(), &error));
  EXPECT_FALSE(error.empty());
}

TEST_F(SigninScreenPolicyProviderTest, AllowEssentialExtension) {
  // Essential component extensions for the login screen should always work.
  scoped_refptr<const extensions::Extension> extension = CreateTestApp(
      extension_misc::kGnubbyAppId, ManifestLocation::kExternalComponent);
  std::u16string error;
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error));
  EXPECT_TRUE(error.empty());
}

TEST_F(SigninScreenPolicyProviderTest, AllowWhitelistedExtensionViaPolicy) {
  // Whitelisted Google-developed extensions should be available if installed
  // via policy. This test should be changed in future as we evolve feature
  // requirements.
  extensions::ScopedCurrentChannel channel(version_info::Channel::STABLE);
  scoped_refptr<const extensions::Extension> extension = CreateTestApp(
      kSampleSigninExtensionId, ManifestLocation::kExternalPolicy);
  std::u16string error;
  EXPECT_TRUE(provider_.UserMayLoad(extension.get(), &error));
  EXPECT_TRUE(error.empty());
}

TEST_F(SigninScreenPolicyProviderTest, DenyNonPolicyWhitelistedExtension) {
  // Google-developed extensions, if not installed via policy, should
  // be disabled.
  extensions::ScopedCurrentChannel channel(version_info::Channel::STABLE);
  scoped_refptr<const extensions::Extension> extension = CreateTestApp(
      kSampleSigninExtensionId, ManifestLocation::kExternalComponent);
  std::u16string error;
  EXPECT_FALSE(provider_.UserMayLoad(extension.get(), &error));
  EXPECT_FALSE(error.empty());
}

TEST_F(SigninScreenPolicyProviderTest, DenyRandomNonPolicyExtension) {
  scoped_refptr<const extensions::Extension> extension =
      CreateTestApp(kRandomExtensionId, ManifestLocation::kExternalComponent);
  std::u16string error;
  EXPECT_FALSE(provider_.UserMayLoad(extension.get(), &error));
  EXPECT_FALSE(error.empty());
}
