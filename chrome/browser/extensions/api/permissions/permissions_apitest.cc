// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_with_management_policy_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

namespace {

static void AddPattern(URLPatternSet* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

}  // namespace

using ContextType = ExtensionBrowserTest::ContextType;

class ExperimentalApiTest : public ExtensionApiTest {
 public:
  ExperimentalApiTest() = default;
  ~ExperimentalApiTest() override = default;
  ExperimentalApiTest(const ExperimentalApiTest&) = delete;
  ExperimentalApiTest& operator=(const ExperimentalApiTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }
};

class PermissionsApiTest : public ExtensionApiTest {
 public:
 public:
  explicit PermissionsApiTest(ContextType context_type = ContextType::kNone)
      : ExtensionApiTest(context_type) {}
  ~PermissionsApiTest() override = default;
  PermissionsApiTest(const PermissionsApiTest&) = delete;
  PermissionsApiTest& operator=(const PermissionsApiTest&) = delete;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

class PermissionsApiTestWithContextType
    : public PermissionsApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  PermissionsApiTestWithContextType() : PermissionsApiTest(GetParam()) {}
  ~PermissionsApiTestWithContextType() override = default;
  PermissionsApiTestWithContextType(const PermissionsApiTestWithContextType&) =
      delete;
  PermissionsApiTestWithContextType& operator=(
      const PermissionsApiTestWithContextType&) = delete;
};

IN_PROC_BROWSER_TEST_P(PermissionsApiTestWithContextType, PermissionsFail) {
  ASSERT_TRUE(RunExtensionTest("permissions/disabled")) << message_;

  // Since the experimental APIs require a flag, this will fail even though
  // it's enabled.
  // TODO(erikkay) This test is currently broken because LoadExtension in
  // ExtensionBrowserTest doesn't actually fail, it just times out.  To fix this
  // I'll need to add an EXTENSION_LOAD_ERROR notification, which is probably
  // too much for the branch.  I'll enable this on trunk later.
  // ASSERT_FALSE(RunExtensionTest("permissions/enabled"))) << message_;
}

IN_PROC_BROWSER_TEST_F(ExperimentalApiTest, PermissionsSucceed) {
  ASSERT_TRUE(RunExtensionTest("permissions/enabled")) << message_;
}

IN_PROC_BROWSER_TEST_P(PermissionsApiTestWithContextType,
                       ExperimentalPermissionsFail) {
  // At the time this test is being created, there is no experimental
  // function that will not be graduating soon, and does not require a
  // tab id as an argument.  So, we need the tab permission to get
  // a tab id.
  ASSERT_TRUE(RunExtensionTest("permissions/experimental_disabled"))
      << message_;
}

// TODO(crbug.com/40124130): Flaky on ChromeOS, Linux, and Mac non-dbg builds.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)) && \
    defined(NDEBUG)
#define MAYBE_FaviconPermission DISABLED_FaviconPermission
#else
#define MAYBE_FaviconPermission FaviconPermission
#endif
IN_PROC_BROWSER_TEST_F(PermissionsApiTest, MAYBE_FaviconPermission) {
  ASSERT_TRUE(RunExtensionTest("permissions/favicon")) << message_;
}

// Test functions and APIs that are always allowed (even if you ask for no
// permissions).
IN_PROC_BROWSER_TEST_F(PermissionsApiTest, AlwaysAllowed) {
  ASSERT_TRUE(RunExtensionTest("permissions/always_allowed")) << message_;
}

// Tests that the optional permissions API works correctly.
IN_PROC_BROWSER_TEST_P(PermissionsApiTestWithContextType,
                       OptionalPermissionsGranted) {
  // Mark all the tested APIs as granted to bypass the confirmation UI.
  APIPermissionSet apis;
  apis.insert(extensions::mojom::APIPermissionID::kBookmark);
  URLPatternSet explicit_hosts;
  AddPattern(&explicit_hosts, "http://*.c.com/*");

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser()->profile());
  prefs->AddRuntimeGrantedPermissions(
      "kjmkgkdkpedkejedfhmfcenooemhbpbo",
      PermissionSet(std::move(apis), ManifestPermissionSet(),
                    std::move(explicit_hosts), URLPatternSet()));

  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  ASSERT_TRUE(StartEmbeddedTestServer());
  EXPECT_TRUE(RunExtensionTest("permissions/optional")) << message_;
}

// Tests that the optional permissions API works correctly.
IN_PROC_BROWSER_TEST_P(PermissionsApiTestWithContextType,
                       OptionalPermissionsAutoConfirm) {
  // Rather than setting the granted permissions, set the UI autoconfirm flag
  // and run the same tests.
  auto dialog_action_reset =
      PermissionsRequestFunction::SetDialogActionForTests(
          PermissionsRequestFunction::DialogAction::kAutoConfirm);
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  ASSERT_TRUE(StartEmbeddedTestServer());
  EXPECT_TRUE(RunExtensionTest("permissions/optional")) << message_;
}

// Test that denying the optional permissions confirmation dialog works.
IN_PROC_BROWSER_TEST_F(PermissionsApiTest, OptionalPermissionsDeny) {
  // Mark the management permission as already granted since we auto reject
  // user prompts.
  APIPermissionSet apis;
  apis.insert(mojom::APIPermissionID::kManagement);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser()->profile());
  prefs->AddRuntimeGrantedPermissions(
      "kjmkgkdkpedkejedfhmfcenooemhbpbo",
      PermissionSet(std::move(apis), ManifestPermissionSet(), URLPatternSet(),
                    URLPatternSet()));

  auto dialog_action_reset =
      PermissionsRequestFunction::SetDialogActionForTests(
          PermissionsRequestFunction::DialogAction::kAutoReject);
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  ASSERT_TRUE(StartEmbeddedTestServer());
  EXPECT_TRUE(RunExtensionTest("permissions/optional_deny")) << message_;
}

// Tests that the permissions.request function must be called from within a
// user gesture.
IN_PROC_BROWSER_TEST_P(PermissionsApiTestWithContextType,
                       OptionalPermissionsGesture) {
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(false);
  ASSERT_TRUE(StartEmbeddedTestServer());
  EXPECT_TRUE(RunExtensionTest("permissions/optional_gesture")) << message_;
}

// Tests that the user gesture is retained in the permissions.request function
// callback.
IN_PROC_BROWSER_TEST_F(PermissionsApiTest, OptionalPermissionsRetainGesture) {
  auto dialog_action_reset =
      PermissionsRequestFunction::SetDialogActionForTests(
          PermissionsRequestFunction::DialogAction::kAutoConfirm);
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(false);
  ASSERT_TRUE(StartEmbeddedTestServer());
  EXPECT_TRUE(RunExtensionTest("permissions/optional_retain_gesture"))
      << message_;
}

// Test that optional permissions blocked by enterprise policy will be denied
// automatically.
IN_PROC_BROWSER_TEST_F(ExtensionApiTestWithManagementPolicy,
                       OptionalPermissionsPolicyBlocked) {
  // Set enterprise policy to block some API permissions.
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddBlockedPermission("*", "management");
  }
  // Set auto confirm UI flag.
  auto dialog_action_reset =
      PermissionsRequestFunction::SetDialogActionForTests(
          PermissionsRequestFunction::DialogAction::kAutoConfirm);
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  EXPECT_TRUE(RunExtensionTest("permissions/optional_policy_blocked"))
      << message_;
}

// Tests that an extension can't gain access to file: URLs without the checkbox
// entry in prefs. There shouldn't be a warning either.
IN_PROC_BROWSER_TEST_F(PermissionsApiTest, OptionalPermissionsFileAccess) {
  // There shouldn't be a warning, so we shouldn't need to autoconfirm.
  auto dialog_action_reset =
      PermissionsRequestFunction::SetDialogActionForTests(
          PermissionsRequestFunction::DialogAction::kAutoReject);
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser()->profile());

  EXPECT_TRUE(RunExtensionTest("permissions/file_access_no")) << message_;
  EXPECT_FALSE(prefs->AllowFileAccess(last_loaded_extension_id()));
  EXPECT_TRUE(RunExtensionTest("permissions/file_access_yes", {},
                               {.allow_file_access = true}))
      << message_;
  EXPECT_TRUE(prefs->AllowFileAccess(last_loaded_extension_id()));
}

// Tests loading of files or directory listings when an extension has file
// access.
IN_PROC_BROWSER_TEST_F(PermissionsApiTest, FileLoad) {
  base::ScopedTempDir temp_dir;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath empty_file = temp_dir.GetPath().AppendASCII("empty.html");
    base::FilePath original_empty_file = ui_test_utils::GetTestFilePath(
        base::FilePath(), base::FilePath().AppendASCII("empty.html"));

    EXPECT_TRUE(base::PathExists(original_empty_file));
    EXPECT_TRUE(base::CopyFile(original_empty_file, empty_file));
  }
  EXPECT_TRUE(RunExtensionTest(
      "permissions/file_load",
      {.custom_arg = temp_dir.GetPath().MaybeAsASCII().c_str()},
      {.allow_file_access = true}))
      << message_;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir.Delete());
  }
}

// Test requesting, querying, and removing host permissions for host
// permissions that are a subset of the optional permissions.
IN_PROC_BROWSER_TEST_P(PermissionsApiTestWithContextType, HostSubsets) {
  auto dialog_action_reset =
      PermissionsRequestFunction::SetDialogActionForTests(
          PermissionsRequestFunction::DialogAction::kAutoConfirm);
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  EXPECT_TRUE(RunExtensionTest("permissions/host_subsets")) << message_;
}

// Tests that requesting an optional permission from a background page, with
// another window open, grants the permission and updates the bindings
// (chrome.whatever, in this case chrome.alarms). Regression test for
// crbug.com/435141, see details there for trickiness.
IN_PROC_BROWSER_TEST_F(PermissionsApiTest, OptionalPermissionsUpdatesBindings) {
  ASSERT_TRUE(RunExtensionTest("permissions/optional_updates_bindings"))
      << message_;
}

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         PermissionsApiTestWithContextType,
                         testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         PermissionsApiTestWithContextType,
                         testing::Values(ContextType::kServiceWorker));

class PermissionsApiSiteAccessRequestsTest : public PermissionsApiTest {
 public:
  PermissionsApiSiteAccessRequestsTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kApiPermissionsSiteAccessRequests);
  }
  ~PermissionsApiSiteAccessRequestsTest() override = default;
  PermissionsApiSiteAccessRequestsTest(
      const PermissionsApiSiteAccessRequestsTest&) = delete;
  PermissionsApiSiteAccessRequestsTest& operator=(
      const PermissionsApiSiteAccessRequestsTest&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PermissionsApiSiteAccessRequestsTest,
                       InvalidAddSiteAccessRequests) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  ASSERT_TRUE(RunExtensionTest("permissions/add_site_access_request"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PermissionsApiSiteAccessRequestsTest,
                       InvalidRemoveSiteAccessRequests) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  ASSERT_TRUE(RunExtensionTest("permissions/remove_site_access_request"))
      << message_;
}

}  // namespace extensions
