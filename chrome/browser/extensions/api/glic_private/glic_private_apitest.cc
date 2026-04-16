// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/api/glic_private/glic_private_api.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest.h"

namespace extensions {

namespace {
// The extension ID should match the one in manifest.json.
constexpr char kExtensionId[] = "admccjkmockfdflocgggjfgdacdodkdf";
}  // namespace

class GlicPrivateApiTest : public ExtensionApiTest {
 public:
  GlicPrivateApiTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{extensions_features::kApiGlicPrivate, {}},
         {extensions_features::kApiGlicAccessFromGoogleWebpage, {}},
         {features::kGlicActor,
          {{"glic_actor_policy_control_exemption", "true"}}}},
        {});
    ComponentLoader::EnableBackgroundExtensionsForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class GlicPrivateApiFullyEnabledTest : public GlicPrivateApiTest {
 private:
  glic::GlicTestEnvironment glic_test_environment_;
};

IN_PROC_BROWSER_TEST_F(GlicPrivateApiFullyEnabledTest, GetState) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(kExtensionId);
  EXPECT_TRUE(RunExtensionTest("glic_private", {.custom_arg = "fully_enabled"},
                               {.load_as_component = true}))
      << message_;
}

class GlicPrivateApiDisabledTest : public GlicPrivateApiTest {
 private:
  glic::GlicTestEnvironment glic_test_environment_{
      {.force_signin_and_glic_capability = false}};
};

IN_PROC_BROWSER_TEST_F(GlicPrivateApiDisabledTest, GetState) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(kExtensionId);
  EXPECT_TRUE(RunExtensionTest("glic_private", {.custom_arg = "disabled"},
                               {.load_as_component = true}))
      << message_;
}

class GlicPrivateApiNotReadyTest : public GlicPrivateApiTest {
 private:
  glic::GlicTestEnvironment glic_test_environment_{
      {.fre_status = glic::prefs::FreStatus::kNotStarted}};
};

IN_PROC_BROWSER_TEST_F(GlicPrivateApiNotReadyTest, GetState) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(kExtensionId);
  EXPECT_TRUE(RunExtensionTest("glic_private", {.custom_arg = "not_ready"},
                               {.load_as_component = true}))
      << message_;
}

class GlicPrivateApiFeatureDisabledTest : public ExtensionApiTest {
 public:
  GlicPrivateApiFeatureDisabledTest() {
    scoped_feature_list_.InitWithFeatures(
        {}, {extensions_features::kApiGlicPrivate, features::kGlicActor});
    ComponentLoader::EnableBackgroundExtensionsForTesting();
  }

 private:
  glic::GlicTestEnvironment glic_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicPrivateApiFeatureDisabledTest, GetState) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(kExtensionId);
  // The API should be undefined when the feature is disabled.
  EXPECT_TRUE(RunExtensionTest("glic_private",
                               {.custom_arg = "feature_disabled"},
                               {.load_as_component = true}))
      << message_;
}

// Invoke is not supported in Android yet.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicPrivateApiFullyEnabledTest, Invoke) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(kExtensionId);
  EXPECT_TRUE(RunExtensionTest(
      "glic_private", {.extension_url = "test.html", .custom_arg = "invoke"},
      {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(GlicPrivateApiFullyEnabledTest, InvokeInNewTab) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(kExtensionId);

  int initial_tab_count = browser()->tab_strip_model()->count();

  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "invoke_new_tab"},
      {.load_as_component = true}))
      << message_;

  // Verify that at least one new tab was created.
  // Note: The test may run twice (in service worker and page), opening 2 tabs.
  EXPECT_GE(browser()->tab_strip_model()->count(), initial_tab_count + 1);
}

IN_PROC_BROWSER_TEST_F(GlicPrivateApiDisabledTest, Invoke) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(kExtensionId);
  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "invoke_disabled"},
      {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(GlicPrivateApiFeatureDisabledTest, Invoke) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(kExtensionId);
  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "invoke_feature_disabled"},
      {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(GlicPrivateApiNotReadyTest, Invoke) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(kExtensionId);
  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "invoke_not_ready"},
      {.load_as_component = true}))
      << message_;
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace extensions
