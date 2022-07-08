// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/offscreen/offscreen_api.h"

#include <algorithm>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

class OffscreenApiTest : public ExtensionApiTest {
 public:
  OffscreenApiTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionsOffscreenDocuments);
  }
  ~OffscreenApiTest() override = default;

 private:
  // The `offscreen` API is currently behind both a feature and a channel
  // restriction.
  base::test::ScopedFeatureList feature_list_;
  ScopedCurrentChannel current_channel_override_{
      version_info::Channel::UNKNOWN};
};

// Tests the general flow of creating an offscreen document.
IN_PROC_BROWSER_TEST_F(OffscreenApiTest, BasicDocumentManagement) {
  ASSERT_TRUE(RunExtensionTest("offscreen/basic_document_management"))
      << message_;
}

class OffscreenApiTestWithoutFeature : public ExtensionApiTest {
 public:
  OffscreenApiTestWithoutFeature() = default;
  ~OffscreenApiTestWithoutFeature() override = default;

 private:
  ScopedCurrentChannel current_channel_override_{
      version_info::Channel::UNKNOWN};
};

// Tests that the `offscreen` API is unavailable if the requisite feature
// (`ExtensionsOffscreenDocuments`) is not enabled. We have this explicit test
// mostly to double-check our registration, since features are prone to typos.
IN_PROC_BROWSER_TEST_F(OffscreenApiTestWithoutFeature,
                       APIUnavailableWithoutFeature) {
  static constexpr char kManifest[] =
      R"({
           "name": "Offscreen Document Test",
           "manifest_version": 3,
           "version": "0.1",
           "permissions": ["offscreen"],
           "background": { "service_worker": "background.js" }
         })";
  // The extension validates the `offscreen` API is undefined.
  static constexpr char kBackgroundJs[] =
      R"(chrome.test.runTests([
           function apiIsUnavailable() {
             chrome.test.assertEq(undefined, chrome.offscreen);
             chrome.test.succeed();
           },
         ]);)";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);

  ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(
      test_dir.UnpackedPath(), {.ignore_manifest_warnings = true});
  ASSERT_TRUE(extension);

  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  // An install warning should be emitted since the extension requested a
  // restricted permission.
  const std::vector<InstallWarning>& install_warnings =
      extension->install_warnings();

  // Turn our InstallWarnings into strings for easier testing.
  std::vector<std::string> string_warnings;
  std::transform(install_warnings.begin(), install_warnings.end(),
                 std::back_inserter(string_warnings),
                 [](const InstallWarning& warning) { return warning.message; });

  static constexpr char kExpectedWarning[] =
      "'offscreen' requires the 'ExtensionsOffscreenDocuments' feature flag to "
      "be enabled.";
  EXPECT_THAT(string_warnings, testing::ElementsAre(kExpectedWarning));
}

}  // namespace extensions
