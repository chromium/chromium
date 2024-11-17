// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/permissions/permissions_api.h"

#include <memory>
#include <optional>
#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/permissions/permissions_test_util.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

constexpr char kNotInManifestError[] =
    "Only permissions specified in the manifest may be requested.";

using permissions_test_util::GetPatternsAsStrings;

scoped_refptr<const Extension> CreateExtensionWithPermissions(
    base::Value::List permissions,
    const std::string& name,
    bool allow_file_access) {
  int creation_flags = Extension::NO_FLAGS;
  if (allow_file_access)
    creation_flags |= Extension::ALLOW_FILE_ACCESS;
  return ExtensionBuilder()
      .SetLocation(mojom::ManifestLocation::kInternal)
      .SetManifest(base::Value::Dict()
                       .Set("name", name)
                       .Set("description", "foo")
                       .Set("manifest_version", 2)
                       .Set("version", "0.1.2.3")
                       .Set("permissions", std::move(permissions)))
      .AddFlags(creation_flags)
      .SetID(crx_file::id_util::GenerateId(name))
      .Build();
}

// Runs permissions.request() with the provided |args|, and returns the result
// of the API call. Expects the function to succeed.
// Populates |did_prompt_user| with whether the user would be prompted for the
// new permissions.
bool RunRequestFunction(
    const Extension& extension,
    content::BrowserContext* browser_context,
    const char* args,
    std::unique_ptr<const PermissionSet>* prompted_permissions_out) {
  auto function = base::MakeRefCounted<PermissionsRequestFunction>();
  function->set_user_gesture(true);
  function->set_extension(&extension);
  std::optional<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), args, browser_context,
          api_test_utils::FunctionMode::kNone);
  if (!function->GetError().empty()) {
    ADD_FAILURE() << "Unexpected function error: " << function->GetError();
    return false;
  }

  if (!result || !result->is_bool()) {
    ADD_FAILURE() << "Unexpected function result.";
    return false;
  }

  *prompted_permissions_out = function->TakePromptedPermissionsForTesting();

  return result->GetBool();
}

}  // namespace

class PermissionsAPIUnitTest : public ExtensionServiceTestWithInstall {
 public:
  PermissionsAPIUnitTest() {}

  PermissionsAPIUnitTest(const PermissionsAPIUnitTest&) = delete;
  PermissionsAPIUnitTest& operator=(const PermissionsAPIUnitTest&) = delete;

  ~PermissionsAPIUnitTest() override {}
  Browser* browser() { return browser_.get(); }

  // Runs chrome.permissions.contains(|json_query|).
  bool RunContainsFunction(const std::string& manifest_permission,
                           const std::string& args_string,
                           bool allow_file_access) {
    SCOPED_TRACE(args_string);
    scoped_refptr<const Extension> extension = CreateExtensionWithPermissions(
        base::Value::List().Append(manifest_permission), "My Extension",
        allow_file_access);
    ExtensionPrefs::Get(profile())->SetAllowFileAccess(extension->id(),
                                                       allow_file_access);
    scoped_refptr<PermissionsContainsFunction> function(
        new PermissionsContainsFunction());
    function->set_extension(extension.get());
    bool run_result =
        api_test_utils::RunFunction(function.get(), args_string, profile(),
                                    api_test_utils::FunctionMode::kNone);
    EXPECT_TRUE(run_result) << function->GetError();

    const auto& args_list = *function->GetResultListForTest();
    if (args_list.empty()) {
      ADD_FAILURE() << "Result unexpectedly empty.";
      return false;
    }
    if (!args_list[0].is_bool()) {
      ADD_FAILURE() << "Result is not a boolean.";
      return false;
    }
    return args_list[0].GetBool();
  }

  // Adds the extension to the ExtensionService, and grants any initial
  // permissions.
  void AddExtensionAndGrantPermissions(const Extension& extension) {
    PermissionsUpdater updater(profile());
    updater.InitializePermissions(&extension);
    updater.GrantActivePermissions(&extension);
    service()->AddExtension(&extension);
  }

  // Adds the extension to the ExtensionService, and withheld any initial
  // permissions.
  void AddExtensionAndWithheldPermissions(const Extension& extension) {
    PermissionsUpdater updater(profile());
    updater.InitializePermissions(&extension);
    ScriptingPermissionsModifier(profile(), &extension)
        .SetWithholdHostPermissions(true);
    service()->AddExtension(&extension);
  }

 protected:
  // ExtensionServiceTestBase:
  void SetUp() override {
    ExtensionServiceTestWithInstall::SetUp();
    dialog_action_ = PermissionsRequestFunction::SetDialogActionForTests(
        PermissionsRequestFunction::DialogAction::kAutoConfirm);
    InitializeEmptyExtensionService();
    browser_window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(profile(), true);
    params.type = Browser::TYPE_NORMAL;
    params.window = browser_window_.get();
    browser_.reset(Browser::Create(params));
  }
  // ExtensionServiceTestBase:
  void TearDown() override {
    dialog_action_.reset();
    browser_.reset();
    browser_window_.reset();
    ExtensionServiceTestWithInstall::TearDown();
  }

 private:
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;
  std::optional<base::AutoReset<PermissionsRequestFunction::DialogAction>>
      dialog_action_;
};

TEST_F(PermissionsAPIUnitTest, Contains) {
  // 1. Since the extension does not have file:// origin access, expect it
  // to return false;
  bool expected_has_permission = false;
  bool has_permission = RunContainsFunction(
      "tabs", "[{\"origins\":[\"file://*\"]}]", false /* allow_file_access */);
  EXPECT_EQ(expected_has_permission, has_permission);

  // 2. Extension has tabs permission, expect to return true.
  expected_has_permission = true;
  has_permission = RunContainsFunction("tabs", "[{\"permissions\":[\"tabs\"]}]",
                                       false /* allow_file_access */);
  EXPECT_EQ(expected_has_permission, has_permission);

  // 3. Extension has file permission, but not active. Expect to return false.
  expected_has_permission = false;
  has_permission =
      RunContainsFunction("file://*", "[{\"origins\":[\"file://*\"]}]",
                          false /* allow_file_access */);
  EXPECT_EQ(expected_has_permission, has_permission);

  // 4. Same as #3, but this time with file access allowed.
  expected_has_permission = true;
  has_permission =
      RunContainsFunction("file:///*", "[{\"origins\":[\"file:///*\"]}]",
                          true /* allow_file_access */);
  EXPECT_EQ(expected_has_permission, has_permission);

  // Tests calling contains() with <all_urls> with and without file access.
  // Regression test for https://crbug.com/931816.
  EXPECT_TRUE(RunContainsFunction("<all_urls>",
                                  R"([{"origins": ["<all_urls>"]}])",
                                  false /* allow file access */));
  EXPECT_TRUE(RunContainsFunction("<all_urls>",
                                  R"([{"origins": ["<all_urls>"]}])",
                                  true /* allow file access */));
}

TEST_F(PermissionsAPIUnitTest, ContainsAndGetAllWithRuntimeHostPermissions) {
  constexpr char kExampleCom[] = "https://example.com/*";
  constexpr char kContentScriptCom[] = "https://contentscript.com/*";
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddHostPermission(kExampleCom)
          .AddContentScript("foo.js", {kContentScriptCom, kExampleCom})
          .Build();

  AddExtensionAndGrantPermissions(*extension);
  PermissionsUpdater updater(profile());
  updater.InitializePermissions(extension.get());
  updater.GrantActivePermissions(extension.get());
  service()->AddExtension(extension.get());

  auto contains_origin = [this, &extension](const char* origin) {
    SCOPED_TRACE(origin);
    auto function = base::MakeRefCounted<PermissionsContainsFunction>();
    function->set_extension(extension.get());
    if (!api_test_utils::RunFunction(
            function.get(),
            base::StringPrintf(R"([{"origins": ["%s"]}])", origin), profile(),
            api_test_utils::FunctionMode::kNone)) {
      ADD_FAILURE() << "Running function failed: " << function->GetError();
    }

    return (*function->GetResultListForTest())[0].GetBool();
  };

  auto get_all = [this, &extension]() {
    auto function = base::MakeRefCounted<PermissionsGetAllFunction>();
    function->set_extension(extension.get());

    std::vector<std::string> origins;
    if (!api_test_utils::RunFunction(function.get(), "[]", profile(),
                                     api_test_utils::FunctionMode::kNone)) {
      ADD_FAILURE() << "Running function failed: " << function->GetError();
      return origins;
    }

    const base::Value::List* results = function->GetResultListForTest();
    if (results->size() != 1u || !(*results)[0].is_dict()) {
      ADD_FAILURE() << "Invalid result value";
      return origins;
    }

    const base::Value::List* origins_value =
        (*results)[0].GetDict().FindList("origins");
    for (const auto& value : *origins_value) {
      origins.push_back(value.GetString());
    }

    return origins;
  };

  // Currently, the extension should have access to example.com and
  // contentscript.com (since permissions are not withheld).
  EXPECT_TRUE(contains_origin(kExampleCom));
  EXPECT_TRUE(contains_origin(kContentScriptCom));
  EXPECT_THAT(get_all(),
              testing::UnorderedElementsAre(kExampleCom, kContentScriptCom));

  ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);

  // Once we withhold the permission, the contains function should correctly
  // report the value.
  EXPECT_FALSE(contains_origin(kExampleCom));
  EXPECT_FALSE(contains_origin(kContentScriptCom));
  EXPECT_THAT(get_all(), testing::IsEmpty());

  constexpr char kChromiumOrg[] = "https://chromium.org/";
  modifier.GrantHostPermission(GURL(kChromiumOrg));

  // The permissions API only reports active permissions, rather than granted
  // permissions. This means it will not report values for permissions that
  // aren't requested. This is probably good, because the extension wouldn't be
  // able to use them anyway (since they aren't active).
  EXPECT_FALSE(contains_origin(kChromiumOrg));
  EXPECT_THAT(get_all(), testing::IsEmpty());

  // Fun edge case: example.com is requested as both a scriptable and an
  // explicit host. It is technically possible that it may be granted *only* as
  // one of the two (e.g., only explicit granted).
  {
    URLPatternSet explicit_hosts(
        {URLPattern(Extension::kValidHostPermissionSchemes, kExampleCom)});
    permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
        profile(), *extension,
        PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                      std::move(explicit_hosts), URLPatternSet()));
    const GURL example_url("https://example.com");
    const PermissionSet& active_permissions =
        extension->permissions_data()->active_permissions();
    EXPECT_TRUE(active_permissions.explicit_hosts().MatchesURL(example_url));
    EXPECT_FALSE(active_permissions.scriptable_hosts().MatchesURL(example_url));
  }
  // In this case, contains() should return *false* (because not all the
  // permissions are active, but getAll() should include example.com (because it
  // has been [partially] granted). In practice, this case should be
  // exceptionally rare, and we're mostly just making sure that there's some
  // sane behavior.
  EXPECT_FALSE(contains_origin(kExampleCom));
  EXPECT_THAT(get_all(), testing::ElementsAre(kExampleCom));
}

// Tests requesting permissions that are already granted with the
// permissions.request() API.
TEST_F(PermissionsAPIUnitTest, RequestingGrantedPermissions) {
  // Create an extension with requires all urls, and grant the permission.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension").AddHostPermission("<all_urls>").Build();
  AddExtensionAndGrantPermissions(*extension);

  // Request access to any host permissions. No permissions should be prompted,
  // since permissions that are already granted are not taken into account.
  std::unique_ptr<const PermissionSet> prompted_permissions;
  EXPECT_TRUE(RunRequestFunction(*extension, profile(),
                                 R"([{"origins": ["https://*/*"]}])",
                                 &prompted_permissions));
  EXPECT_EQ(prompted_permissions, nullptr);
}

// Tests requesting withheld permissions with the permissions.request() API.
TEST_F(PermissionsAPIUnitTest, RequestingWithheldPermissions) {
  // Create an extension with required host permissions, and withhold those
  // permissions.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddHostPermissions({"https://example.com/*", "https://google.com/*"})
          .Build();
  AddExtensionAndGrantPermissions(*extension);
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  const GURL kExampleCom("https://example.com");
  const GURL kGoogleCom("https://google.com");
  const PermissionsData* permissions_data = extension->permissions_data();
  EXPECT_TRUE(
      permissions_data->active_permissions().effective_hosts().is_empty());

  // Request one of the withheld permissions.
  std::unique_ptr<const PermissionSet> prompted_permissions;
  EXPECT_TRUE(RunRequestFunction(*extension, profile(),
                                 R"([{"origins": ["https://example.com/*"]}])",
                                 &prompted_permissions));
  ASSERT_TRUE(prompted_permissions);
  EXPECT_THAT(GetPatternsAsStrings(prompted_permissions->effective_hosts()),
              testing::UnorderedElementsAre("https://example.com/*"));

  // The withheld permission should be granted.
  EXPECT_TRUE(
      permissions_data->active_permissions().effective_hosts().MatchesURL(
          kExampleCom));
  EXPECT_FALSE(
      permissions_data->active_permissions().effective_hosts().MatchesURL(
          kGoogleCom));
}

// Tests requesting withheld content script permissions with the
// permissions.request() API.
TEST_F(PermissionsAPIUnitTest, RequestingWithheldContentScriptPermissions) {
  constexpr char kContentScriptPattern[] = "https://contentscript.com/*";
  // Create an extension with required host permissions, and withhold those
  // permissions.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddContentScript("foo.js", {kContentScriptPattern})
          .Build();
  AddExtensionAndGrantPermissions(*extension);
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  const GURL kContentScriptCom("https://contentscript.com");
  const PermissionsData* permissions_data = extension->permissions_data();
  EXPECT_TRUE(
      permissions_data->active_permissions().effective_hosts().is_empty());

  // Request one of the withheld permissions.
  std::unique_ptr<const PermissionSet> prompted_permissions;
  EXPECT_TRUE(
      RunRequestFunction(*extension, profile(),
                         R"([{"origins": ["https://contentscript.com/*"]}])",
                         &prompted_permissions));
  ASSERT_TRUE(prompted_permissions);
  EXPECT_THAT(GetPatternsAsStrings(prompted_permissions->effective_hosts()),
              testing::UnorderedElementsAre(kContentScriptPattern));

  // The withheld permission should be granted.
  EXPECT_THAT(GetPatternsAsStrings(
                  permissions_data->active_permissions().effective_hosts()),
              testing::UnorderedElementsAre(kContentScriptPattern));
  EXPECT_TRUE(
      permissions_data->withheld_permissions().effective_hosts().is_empty());
}

// Tests requesting a withheld host permission that is both an explicit and a
// scriptable host with the permissions.request() API.
TEST_F(PermissionsAPIUnitTest,
       RequestingWithheldExplicitAndScriptablePermissionsInTheSameCall) {
  constexpr char kContentScriptPattern[] = "https://example.com/*";
  // Create an extension with required host permissions, and withhold those
  // permissions.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddHostPermission("https://example.com/*")
          .AddContentScript("foo.js", {kContentScriptPattern})
          .Build();
  AddExtensionAndGrantPermissions(*extension);
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  const GURL kExampleCom("https://example.com");
  const PermissionsData* permissions_data = extension->permissions_data();
  EXPECT_TRUE(
      permissions_data->active_permissions().effective_hosts().is_empty());

  // Request one of the withheld permissions.
  std::unique_ptr<const PermissionSet> prompted_permissions;
  EXPECT_TRUE(RunRequestFunction(*extension, profile(),
                                 R"([{"origins": ["https://example.com/*"]}])",
                                 &prompted_permissions));
  ASSERT_TRUE(prompted_permissions);
  EXPECT_THAT(GetPatternsAsStrings(prompted_permissions->effective_hosts()),
              testing::UnorderedElementsAre(kContentScriptPattern));

  // The withheld permission should be granted to both explicit and scriptable
  // hosts.
  EXPECT_TRUE(
      permissions_data->active_permissions().explicit_hosts().MatchesURL(
          kExampleCom));
  EXPECT_TRUE(
      permissions_data->active_permissions().scriptable_hosts().MatchesURL(
          kExampleCom));
}

// Tests an extension re-requesting an optional host after the user removes it.
TEST_F(PermissionsAPIUnitTest, ReRequestingWithheldOptionalPermissions) {
  // Create an extension an optional host permissions, and withhold those
  // permissions.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddOptionalHostPermission("https://chromium.org/*")
          .Build();
  AddExtensionAndGrantPermissions(*extension);

  const GURL kChromiumOrg("https://chromium.org");
  const PermissionsData* permissions_data = extension->permissions_data();
  EXPECT_TRUE(
      permissions_data->active_permissions().effective_hosts().is_empty());
  {
    std::unique_ptr<const PermissionSet> prompted_permissions;
    EXPECT_TRUE(RunRequestFunction(
        *extension, profile(), R"([{"origins": ["https://chromium.org/*"]}])",
        &prompted_permissions));
    ASSERT_TRUE(prompted_permissions);
    EXPECT_THAT(GetPatternsAsStrings(prompted_permissions->effective_hosts()),
                testing::UnorderedElementsAre("https://chromium.org/*"));
  }

  EXPECT_TRUE(
      permissions_data->active_permissions().effective_hosts().MatchesURL(
          kChromiumOrg));

  {
    URLPattern chromium_org_pattern(Extension::kValidHostPermissionSchemes,
                                    "https://chromium.org/*");
    PermissionSet permissions(APIPermissionSet(), ManifestPermissionSet(),
                              URLPatternSet({chromium_org_pattern}),
                              URLPatternSet());
    permissions_test_util::RevokeRuntimePermissionsAndWaitForCompletion(
        profile(), *extension, permissions);
  }
  EXPECT_TRUE(
      permissions_data->active_permissions().effective_hosts().is_empty());

  auto dialog_action_reset =
      PermissionsRequestFunction::SetDialogActionForTests(
          PermissionsRequestFunction::DialogAction::kAutoReject);
  {
    std::unique_ptr<const PermissionSet> prompted_permissions;
    EXPECT_FALSE(RunRequestFunction(
        *extension, profile(), R"([{"origins": ["https://chromium.org/*"]}])",
        &prompted_permissions));
    ASSERT_TRUE(prompted_permissions);
    EXPECT_THAT(GetPatternsAsStrings(prompted_permissions->effective_hosts()),
                testing::UnorderedElementsAre("https://chromium.org/*"));
  }
  EXPECT_TRUE(
      permissions_data->active_permissions().effective_hosts().is_empty());
}

// Tests requesting both optional and withheld permissions in the same call to
// permissions.request().
TEST_F(PermissionsAPIUnitTest, RequestingWithheldAndOptionalPermissions) {
  // Create an extension with required and optional host permissions, and
  // withhold the required permissions.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddHostPermissions({"https://example.com/*", "https://google.com/*"})
          .AddOptionalHostPermission("https://chromium.org/*")
          .Build();
  AddExtensionAndGrantPermissions(*extension);
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  const GURL kExampleCom("https://example.com");
  const GURL kGoogleCom("https://google.com");
  const GURL kChromiumOrg("https://chromium.org");
  const PermissionsData* permissions_data = extension->permissions_data();
  EXPECT_TRUE(
      permissions_data->active_permissions().effective_hosts().is_empty());

  // Request one of the withheld host permissions and an optional host
  // permission in the same call.
  std::unique_ptr<const PermissionSet> prompted_permissions;
  EXPECT_TRUE(RunRequestFunction(
      *extension, profile(),
      R"([{"origins": ["https://example.com/*", "https://chromium.org/*"]}])",
      &prompted_permissions));
  ASSERT_TRUE(prompted_permissions);
  EXPECT_THAT(GetPatternsAsStrings(prompted_permissions->effective_hosts()),
              testing::UnorderedElementsAre("https://chromium.org/*",
                                            "https://example.com/*"));

  // The requested permissions should be added.
  EXPECT_TRUE(
      permissions_data->active_permissions().effective_hosts().MatchesURL(
          kExampleCom));
  EXPECT_FALSE(
      permissions_data->active_permissions().effective_hosts().MatchesURL(
          kGoogleCom));
  EXPECT_TRUE(
      permissions_data->active_permissions().effective_hosts().MatchesURL(
          kChromiumOrg));
}

// Tests requesting permissions that weren't specified in the manifest (either
// in optional permissions or in required permissions).
TEST_F(PermissionsAPIUnitTest, RequestingPermissionsNotSpecifiedInManifest) {
  // Create an extension with required and optional host permissions, and
  // withhold the required permissions.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddHostPermission("https://example.com/*")
          .AddOptionalHostPermission("https://chromium.org/*")
          .Build();
  AddExtensionAndGrantPermissions(*extension);
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  const GURL kExampleCom("https://example.com");
  const GURL kGoogleCom("https://google.com");
  const GURL kChromiumOrg("https://chromium.org");

  // Request permission for an optional and required permission, as well as a
  // permission that wasn't specified in the manifest. The call should fail.
  // Note: Not using RunRequestFunction(), since that expects function success.
  auto function = base::MakeRefCounted<PermissionsRequestFunction>();
  function->set_user_gesture(true);
  function->set_extension(extension.get());
  EXPECT_EQ(kNotInManifestError,
            api_test_utils::RunFunctionAndReturnError(
                function.get(),
                R"([{
               "origins": [
                 "https://example.com/*",
                 "https://chromium.org/*",
                 "https://google.com/*"
               ]
             }])",
                profile(), api_test_utils::FunctionMode::kNone));
}

// Tests requesting withheld permissions that have already been granted.
TEST_F(PermissionsAPIUnitTest, RequestingAlreadyGrantedWithheldPermissions) {
  // Create an extension with required host permissions, withhold host
  // permissions, and then grant one of the hosts.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddHostPermissions({"https://example.com/*", "https://google.com/*"})
          .Build();
  AddExtensionAndGrantPermissions(*extension);
  ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);

  const GURL kExampleCom("https://example.com");
  const GURL kGoogleCom("https://google.com");
  modifier.GrantHostPermission(kExampleCom);

  const PermissionsData* permissions_data = extension->permissions_data();
  EXPECT_TRUE(
      permissions_data->active_permissions().effective_hosts().MatchesURL(
          kExampleCom));
  EXPECT_FALSE(
      permissions_data->active_permissions().effective_hosts().MatchesURL(
          kGoogleCom));

  // Request the already-granted host permission. The function should succeed
  // (without even prompting the user), and the permission should (still) be
  // granted.
  auto dialog_action_reset =
      PermissionsRequestFunction::SetDialogActionForTests(
          PermissionsRequestFunction::DialogAction::kAutoReject);

  std::unique_ptr<const PermissionSet> prompted_permissions;
  EXPECT_TRUE(RunRequestFunction(*extension, profile(),
                                 R"([{"origins": ["https://example.com/*"]}])",
                                 &prompted_permissions));
  ASSERT_FALSE(prompted_permissions);

  // The withheld permission should be granted.
  EXPECT_TRUE(
      permissions_data->active_permissions().effective_hosts().MatchesURL(
          kExampleCom));
  EXPECT_FALSE(
      permissions_data->active_permissions().effective_hosts().MatchesURL(
          kGoogleCom));
}

// Test that requesting chrome:-scheme URLs is disallowed in the permissions
// API.
TEST_F(PermissionsAPIUnitTest, RequestingChromeURLs) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddOptionalHostPermission("<all_urls>")
          .Build();
  AddExtensionAndGrantPermissions(*extension);

  const GURL chrome_url("chrome://settings");

  // By default, the extension should not have access to chrome://settings.
  EXPECT_FALSE(extension->permissions_data()->HasHostPermission(chrome_url));
  // The optional permissions should also omit the chrome:-scheme for the
  // <all_urls> pattern.
  EXPECT_FALSE(PermissionsParser::GetOptionalPermissions(extension.get())
                   .explicit_hosts()
                   .MatchesURL(chrome_url));

  {
    // Trying to request "chrome://settings/*" should fail, since it's not in
    // the optional permissions.
    auto function = base::MakeRefCounted<PermissionsRequestFunction>();
    function->set_user_gesture(true);
    function->set_extension(extension.get());
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function.get(), R"([{"origins": ["chrome://settings/*"]}])", profile(),
        api_test_utils::FunctionMode::kNone);
    EXPECT_EQ(kNotInManifestError, error);
  }
  // chrome://settings should still be restricted.
  EXPECT_FALSE(extension->permissions_data()->HasHostPermission(chrome_url));

  // The extension can request <all_urls>, but it should not grant access to the
  // chrome:-scheme.
  std::unique_ptr<const PermissionSet> prompted_permissions;
  RunRequestFunction(*extension, profile(), R"([{"origins": ["<all_urls>"]}])",
                     &prompted_permissions);
  EXPECT_THAT(GetPatternsAsStrings(prompted_permissions->effective_hosts()),
              testing::UnorderedElementsAre("<all_urls>"));

  EXPECT_FALSE(extension->permissions_data()->HasHostPermission(chrome_url));
  EXPECT_TRUE(extension->permissions_data()->HasHostPermission(
      GURL("https://example.com")));
}

// Tests requesting the a file:-scheme pattern with and without file
// access granted. Regression test for https://crbug.com/932703.
TEST_F(PermissionsAPIUnitTest, RequestingFilePermissions) {
  // We need a "real" extension here, since toggling file access requires
  // reloading the extension to re-initialize permissions.
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Extension",
           "manifest_version": 2,
           "version": "0.1",
           "optional_permissions": ["file:///*"]
         })");
  ChromeTestExtensionLoader loader(profile());
  loader.set_allow_file_access(false);
  scoped_refptr<const Extension> extension =
      loader.LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_FALSE(util::AllowFileAccess(extension->id(), profile()));
  const GURL file_url("file:///foo");
  EXPECT_FALSE(extension->permissions_data()->HasHostPermission(file_url));

  {
    auto function = base::MakeRefCounted<PermissionsRequestFunction>();
    function->set_user_gesture(true);
    function->set_extension(extension.get());
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function.get(), R"([{"origins": ["file:///*"]}])", profile(),
        api_test_utils::FunctionMode::kNone);
    EXPECT_EQ("Extension must have file access enabled to request 'file:///*'.",
              error);
    EXPECT_FALSE(extension->permissions_data()->HasHostPermission(file_url));
  }
  {
    TestExtensionRegistryObserver observer(registry(), extension->id());
    // This will reload the extension, so we need to reset the extension
    // pointer.
    util::SetAllowFileAccess(extension->id(), profile(), true);
    extension = observer.WaitForExtensionLoaded();
    ASSERT_TRUE(extension);
  }

  std::unique_ptr<const PermissionSet> prompted_permissions;
  EXPECT_TRUE(RunRequestFunction(*extension, profile(),
                                 R"([{"origins": ["file:///*"]}])",
                                 &prompted_permissions));
  // Note: There are no permission warnings associated with requesting file
  // URLs (probably because there's a separate toggle to control it already);
  // they are filtered out of the permission ID set when we get permission
  // messages.
  EXPECT_FALSE(prompted_permissions);
  EXPECT_TRUE(extension->permissions_data()->HasHostPermission(file_url));
}

class PermissionsAPISiteAccessRequestsUnitTest : public PermissionsAPIUnitTest {
 public:
  PermissionsAPISiteAccessRequestsUnitTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kApiPermissionsSiteAccessRequests);
  }
  ~PermissionsAPISiteAccessRequestsUnitTest() override = default;
  PermissionsAPISiteAccessRequestsUnitTest(
      const PermissionsAPISiteAccessRequestsUnitTest&) = delete;
  PermissionsAPISiteAccessRequestsUnitTest& operator=(
      const PermissionsAPISiteAccessRequestsUnitTest&) = delete;

  // Navigate to `url` in the current web contents.
  void NavigateTo(const std::string& url) {
    web_contents_tester_->NavigateAndCommit(GURL(url));
  }

  // Returns the function params for permissions.add|removeSiteAccessRequest for
  // a tab.
  std::string GetFunctionParams(int tab_id,
                                const std::string& pattern = std::string()) {
    if (pattern.empty()) {
      return base::StringPrintf(R"([{"tabId": %s}])",
                                base::NumberToString(tab_id).c_str());
    }
    return base::StringPrintf(R"([{"tabId": %s, "pattern": "%s"}])",
                              base::NumberToString(tab_id).c_str(),
                              pattern.c_str());
  }

 protected:
  // PermissionsAPIUnitTest:
  void SetUp() override {
    PermissionsAPIUnitTest::SetUp();

    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    content::WebContents* raw_web_contents = web_contents.get();
    browser()->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                    true);
    web_contents_tester_ = content::WebContentsTester::For(raw_web_contents);
  }

  void TearDown() override {
    // Detach the web contents.
    web_contents_tester_ = nullptr;
    browser()->tab_strip_model()->DetachAndDeleteWebContentsAt(/*index=*/0);

    PermissionsAPIUnitTest::TearDown();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<content::WebContentsTester> web_contents_tester_;
};

// Test extension can add a site access request for a site it has host
// permissions for and has withheld site access.
TEST_F(PermissionsAPISiteAccessRequestsUnitTest,
       AddSiteAccessRequest_RequestedSite) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Extension")
          .AddHostPermission("*://*.requested.com/*")
          .Build();
  AddExtensionAndGrantPermissions(*extension);

  // Open tab on a url requested by the extension.
  NavigateTo("http://www.requested.com");
  int tab_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetActiveWebContents());

  auto* permissions_manager = PermissionsManager::Get(profile());

  // Add site access request when extension has granted site access.
  {
    // Function should fail since extension already has site access.
    auto function =
        base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
    function->set_extension(extension.get());
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function.get(), GetFunctionParams(tab_id), profile());
    EXPECT_EQ(
        "Extension cannot add a site access request for a site it already has "
        "access to.",
        error);

    // Verify site access request was not added.
    EXPECT_FALSE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));
  }

  // Add site access request when extension has withheld site access.
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);
  {
    // Function should succeed since extension can be granted access.
    auto function =
        base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
    function->set_extension(extension.get());
    EXPECT_TRUE(api_test_utils::RunFunction(
        function.get(), GetFunctionParams(tab_id), profile(),
        api_test_utils::FunctionMode::kNone));

    // Verify site access request is active.
    EXPECT_TRUE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));
  }
}

// Test extension can add a site access request with a pattern for a site it has
// host permissions for and has withheld site access. Request is only valid if
// pattern matches the extension's host permissions.
TEST_F(PermissionsAPISiteAccessRequestsUnitTest,
       AddSiteAccessRequestWithPattern_RequestedSite) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Extension")
          .AddHostPermission("*://*.requested.com/*")
          .Build();
  AddExtensionAndWithheldPermissions(*extension);

  // Open tab on a url requested by the extension.
  NavigateTo("http://www.requested.com");
  int tab_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetActiveWebContents());

  auto* permissions_manager = PermissionsManager::Get(profile());

  // Add site access request for a pattern that does not match the extension's
  // host permissions.
  {
    // Function should fail since pattern doesn't match the extension's host
    // permissions.
    auto function =
        base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
    function->set_extension(extension.get());
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function.get(),
        GetFunctionParams(tab_id, "*://www.not-requested.com/*"), profile(),
        api_test_utils::FunctionMode::kNone);
    EXPECT_EQ(
        "Extension cannot add a site access request with a pattern that does "
        "match any of its host permissions.",
        error);

    // Verify site access request was not added.
    EXPECT_FALSE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));
  }

  // Add site access request for a pattern that matches the extension's host
  // permissions and the current site.
  {
    // Function should succeed since pattern matches the extension's host
    // permissions.
    auto function =
        base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
    function->set_extension(extension.get());
    EXPECT_TRUE(api_test_utils::RunFunction(
        function.get(), GetFunctionParams(tab_id, "*://www.requested.com/*"),
        profile(), api_test_utils::FunctionMode::kNone));

    // Verify site access request was not added.
    EXPECT_TRUE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));
  }

  // Add site access request for a pattern that matches the extension's host
  // permissions and does not match the current site but will match on a
  // cross-origin navigation.
  {
    // Function should succeed since extension can be granted access.
    auto function =
        base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
    function->set_extension(extension.get());
    EXPECT_TRUE(api_test_utils::RunFunction(
        function.get(), GetFunctionParams(tab_id, "*://*/path"), profile(),
        api_test_utils::FunctionMode::kNone));

    // Verify site access request was not added. Note that new requests will
    // overridden any existent ones.
    EXPECT_FALSE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));

    // Verify site access request was added when navigating to the same-origin
    // url that matches the pattern.
    NavigateTo("http://www.requested.com/path");
    EXPECT_TRUE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));
  }
}

// Test extension can add a site access request for a site it doesn't have host
// permissions for, but request is not active.
TEST_F(PermissionsAPISiteAccessRequestsUnitTest,
       AddSiteAccessRequest_NonRequestedSite) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Extension")
          .AddHostPermission("*://*.requested.com/*")
          .AddAPIPermission("activeTab")
          .Build();
  AddExtensionAndGrantPermissions(*extension);

  // Open tab on a url not requested by the extension.
  NavigateTo("http://www.not-requested.com");
  int tab_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetActiveWebContents());

  auto* permissions_manager = PermissionsManager::Get(profile());

  // Add site access request.
  {
    // Function should succeed since we don't want to reveal information
    // about the current site to the extension, but request is not added.
    // Even though extension could have access via activeTab, extension can only
    // add access requests for sites it has previously requested host
    // permissions for.
    auto function =
        base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
    function->set_extension(extension.get());
    EXPECT_TRUE(api_test_utils::RunFunction(
        function.get(), GetFunctionParams(tab_id), profile(),
        api_test_utils::FunctionMode::kNone));

    // Verify site access request was not added.
    EXPECT_FALSE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));
  }

  // Add site access request with a pattern that matches the current site but
  // doesn't match the extension's host permissions.
  {
    // Function should fail since pattern doesn't match the extension's host
    // permissions.
    auto function =
        base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
    function->set_extension(extension.get());
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function.get(),
        GetFunctionParams(tab_id, "*://www.not-requested.com/*"), profile(),
        api_test_utils::FunctionMode::kNone);
    EXPECT_EQ(
        "Extension cannot add a site access request with a pattern that does "
        "match any of its host permissions.",
        error);

    // Verify site access request was not added.
    EXPECT_FALSE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));
  }
}

// Test extension cannot add a site access request when it doesn't have any
// host permissions.
TEST_F(PermissionsAPISiteAccessRequestsUnitTest,
       AddSiteAccessRequest_NoHostPermissions) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Extension").AddAPIPermission("activeTab").Build();
  service()->AddExtension(extension.get());

  // Open tab on any url.
  NavigateTo("http://www.example.com");
  int tab_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetActiveWebContents());

  auto* permissions_manager = PermissionsManager::Get(profile());

  // Add site access request. Function should fail since extension doesn't have
  // any host permissions.
  auto function =
      base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
  function->set_extension(extension.get());
  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), GetFunctionParams(tab_id), profile());
  EXPECT_EQ(
      "Extension cannot add a site access request when it does not have any "
      "host permissions.",
      error);

  // Verify site access request was not added.
  EXPECT_FALSE(
      permissions_manager->HasActiveSiteAccessRequest(tab_id, extension->id()));
}

// Test extension can add a site access request for a restricted site, but
// request is not active.
TEST_F(PermissionsAPISiteAccessRequestsUnitTest,
       AddSiteAccessRequest_TabId_RestrictedSite) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Extension")
          .AddHostPermission("*://*.requested.com/*")
          .Build();
  AddExtensionAndGrantPermissions(*extension);

  // Open tab on a url not requested by the extension.
  NavigateTo("chrome://extensions");
  int tab_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetActiveWebContents());

  auto* permissions_manager = PermissionsManager::Get(profile());

  // Add site access request.
  {
    // Function should succeed since we don't want to reveal information
    // about the current site to the extension, but request is not added.
    auto function =
        base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
    function->set_extension(extension.get());
    EXPECT_TRUE(api_test_utils::RunFunction(
        function.get(), GetFunctionParams(tab_id), profile(),
        api_test_utils::FunctionMode::kNone));

    // Verify site access request was not added.
    EXPECT_FALSE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));
  }

  // TODO(crbug.com/330588494): Add tests with `pattern` once parameter is
  // added.
}

// Tests extension can add a site access request for a site where it has
// optional host permissions.
TEST_F(PermissionsAPISiteAccessRequestsUnitTest,
       AddSiteAccessRequest_OptionalHostPermissions) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Extension")
          .SetManifestKey("optional_host_permissions",
                          base::Value::List().Append("*://*.optional.com/*"))
          .Build();
  service()->AddExtension(extension.get());

  auto* permissions_manager = PermissionsManager::Get(profile());

  // Navigate to url requested by the extension via optional host permissions.
  // Verify there is no site access request.
  NavigateTo("http://www.optional.com");
  int tab_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_FALSE(
      permissions_manager->HasActiveSiteAccessRequest(tab_id, extension->id()));

  // Add site access request for tab with optional.com. Function should
  // succeed since extension can be granted access.
  auto function =
      base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
  function->set_extension(extension.get());
  EXPECT_TRUE(api_test_utils::RunFunction(function.get(),
                                          GetFunctionParams(tab_id), profile(),
                                          api_test_utils::FunctionMode::kNone));

  // Verify site access request was added.
  EXPECT_TRUE(
      permissions_manager->HasActiveSiteAccessRequest(tab_id, extension->id()));
}

// Tests extension can add a site access request for a site where it wants to
// inject a content script.
TEST_F(PermissionsAPISiteAccessRequestsUnitTest,
       AddSiteAccessRequest_ContentScriptMatches) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Extension")
          .AddContentScript("script.js", {"*://*.contentscript.com/*"})
          .Build();
  AddExtensionAndWithheldPermissions(*extension);

  auto* permissions_manager = PermissionsManager::Get(profile());

  // Navigate to url requested by the extension via the content script. Verify
  // there is no site access request.
  NavigateTo("http://www.contentscript.com");
  int tab_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_FALSE(
      permissions_manager->HasActiveSiteAccessRequest(tab_id, extension->id()));

  // Add site access request for tab with contentscript.com. Function should
  // succeed since extension can be granted access.
  auto function =
      base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
  function->set_extension(extension.get());
  EXPECT_TRUE(api_test_utils::RunFunction(function.get(),
                                          GetFunctionParams(tab_id), profile(),
                                          api_test_utils::FunctionMode::kNone));

  // Verify site access request was added.
  EXPECT_TRUE(
      permissions_manager->HasActiveSiteAccessRequest(tab_id, extension->id()));
}

// Tests extension can add a site access request for a site with access
// withheld, even if the site was blocked by the user. Having a valid request
// doesn't mean it will be signaled to the user.
TEST_F(PermissionsAPISiteAccessRequestsUnitTest,
       AddSiteAccessRequest_UserBlockedSite) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Extension")
          .SetManifestKey("host_permissions",
                          base::Value::List().Append("*://*.requested.com/*"))
          .Build();
  AddExtensionAndGrantPermissions(*extension);

  // Navigate to url requested by the extension.
  NavigateTo("http://www.requested.com");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // Block all extensions on requested.com.
  auto* permissions_manager = PermissionsManager::Get(profile());
  permissions_manager->UpdateUserSiteSetting(
      url::Origin::Create(web_contents->GetLastCommittedURL()),
      PermissionsManager::UserSiteSetting::kBlockAllExtensions);

  // Add site access request for tab with requested.com. Request is invalid
  // because extension has granted site access, even though it can't access the
  // site since user blocked access for all extensions.
  {
    auto function =
        base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
    function->set_extension(extension.get());
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function.get(), GetFunctionParams(tab_id), profile(),
        api_test_utils::FunctionMode::kNone);
    EXPECT_EQ(
        "Extension cannot add a site access request for a site it already has "
        "access to.",
        error);
  }

  // Withheld extension's site access.
  ScriptingPermissionsModifier(profile(), extension.get())
      .SetWithholdHostPermissions(true);

  // Add site access request for tab with requested.com. Request is valid
  // because extension wants site access, and site access was withheld. It
  // doesn't matter that extensions are blocked on the site, since that is a
  // user setting.
  {
    auto function =
        base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
    function->set_extension(extension.get());
    EXPECT_TRUE(api_test_utils::RunFunction(
        function.get(), GetFunctionParams(tab_id), profile(),
        api_test_utils::FunctionMode::kNone));
  }

  // Verify site access request was added.
  EXPECT_TRUE(
      permissions_manager->HasActiveSiteAccessRequest(tab_id, extension->id()));
}

// An extension with granted tab permission (via granting activeTab or running
// an extension set on click) can't add a site request.
TEST_F(PermissionsAPISiteAccessRequestsUnitTest,
       AddSiteAccessRequest_OneTimeGrantedAccess) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Extension")
          .SetManifestKey("host_permissions",
                          base::Value::List().Append("*://*.requested.com/*"))
          .Build();
  AddExtensionAndWithheldPermissions(*extension);

  // Navigate to url requested by the extension.
  NavigateTo("http://www.requested.com");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // Grant one-time site access.
  TabHelper::FromWebContents(web_contents)
      ->active_tab_permission_granter()
      ->GrantIfRequested(extension.get());

  // Add site access request for requested.com. Request is invalid because
  // extension already has site access (even if it's just one-time).
  {
    auto function =
        base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
    function->set_extension(extension.get());
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function.get(), GetFunctionParams(tab_id), profile(),
        api_test_utils::FunctionMode::kNone);
    EXPECT_EQ(
        "Extension cannot add a site access request for a site it already has "
        "access to.",
        error);
  }
}

// Test extension can add a site access request for a site it has host
// permissions for and has withheld site access by providing a document id.
// Note: Document id is converted to a tab id by the API after parsing. Thus,
// it's sufficient to test only some bases cases to make sure the documentId is
// properly parsed. Other scenarios are extensively tested using a tab id.
TEST_F(PermissionsAPISiteAccessRequestsUnitTest,
       AddSiteAccessRequest_DocumentId) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Extension")
          .AddHostPermission("*://*.requested.com/*")
          .Build();
  AddExtensionAndGrantPermissions(*extension);

  // Open tab on a url requested by the extension.
  NavigateTo("http://www.requested.com");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  std::string document_id =
      ExtensionApiFrameIdMap::GetDocumentId(web_contents->GetPrimaryMainFrame())
          .ToString();

  auto* permissions_manager = PermissionsManager::Get(profile());
  auto function_params = [](const std::string& document_id) {
    return base::StringPrintf(R"([{"documentId": "%s"}])", document_id.c_str());
  };

  // Add site access request when extension has granted site access.
  {
    // Function should fail since extension already has site access.
    auto function =
        base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
    function->set_extension(extension.get());
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function.get(), function_params(document_id), profile());
    EXPECT_EQ(
        "Extension cannot add a site access request for a site it already has "
        "access to.",
        error);

    // Verify site access request was not added.
    EXPECT_FALSE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));
  }

  // Add site access request when extension has withheld site access.
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);
  {
    // Function should succeed since extension can be granted access.
    auto function =
        base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
    function->set_extension(extension.get());
    EXPECT_TRUE(api_test_utils::RunFunction(
        function.get(), function_params(document_id), profile(),
        api_test_utils::FunctionMode::kNone));

    // Verify site access request is active.
    EXPECT_TRUE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));
  }
}

// Tests extension cannot remove a site access request that doesn't exist.
TEST_F(PermissionsAPISiteAccessRequestsUnitTest,
       RemoveSiteAccessRequest_TabId_Invalid) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Extension")
          .SetManifestKey("host_permissions",
                          base::Value::List().Append("*://*.requested.com/*"))
          .Build();
  AddExtensionAndWithheldPermissions(*extension);

  // Open tab on a url requested by the extension.
  NavigateTo("http://www.requested.com");
  int tab_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetActiveWebContents());

  auto* permissions_manager = PermissionsManager::Get(profile());

  // Extension cannot remove a request when there is no current request.
  {
    auto remove_function =
        base::MakeRefCounted<PermissionsRemoveSiteAccessRequestFunction>();
    remove_function->set_extension(extension.get());
    std::string error = api_test_utils::RunFunctionAndReturnError(
        remove_function.get(), GetFunctionParams(tab_id), profile(),
        api_test_utils::FunctionMode::kNone);
    EXPECT_EQ(
        "Extension cannot remove a site access request that doesn't exist.",
        error);

    // Verify there is no request.
    EXPECT_FALSE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));
  }

  // Extension cannot remove a site access request with a pattern when it
  // doesn't match the active request (that matches all patterns).
  {
    // Add a site access request without a pattern. Not specifying a pattern
    // means request will be shown for all patterns.
    auto add_function =
        base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
    add_function->set_extension(extension.get());
    EXPECT_TRUE(api_test_utils::RunFunction(
        add_function.get(), GetFunctionParams(tab_id), profile(),
        api_test_utils::FunctionMode::kNone));

    // Verify site access request was added.
    EXPECT_TRUE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));

    // Remove a site access request with 'requested.com' pattern. Even though
    // existent request matches all patterns, the removal must exactly match
    // request. We do this because we don't support "all urls but <x>".
    auto remove_function =
        base::MakeRefCounted<PermissionsRemoveSiteAccessRequestFunction>();
    remove_function->set_extension(extension.get());
    std::string error = api_test_utils::RunFunctionAndReturnError(
        remove_function.get(),
        GetFunctionParams(tab_id, /*pattern=*/"*://*.requested.com/*"),
        profile(), api_test_utils::FunctionMode::kNone);
    EXPECT_EQ(
        "Extension cannot remove a site access request that doesn't exist.",
        error);

    // Verify request wasn't removed.
    EXPECT_TRUE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));
  }

  // Extension cannot remove a site access request with a pattern when it
  // doesn't match the active request (with a different pattern specified).
  {
    // Add a site access request with 'requested.com' pattern. Adding a new
    // request overrides existent request.
    auto add_function =
        base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
    add_function->set_extension(extension.get());
    EXPECT_TRUE(api_test_utils::RunFunction(
        add_function.get(), GetFunctionParams(tab_id, "*://*.requested.com/*"),
        profile(), api_test_utils::FunctionMode::kNone));

    // Verify site access request was added.
    EXPECT_TRUE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));

    // Remove a site access request with a 'other.com' pattern. Function is
    // invalid because 'other.com' doesn't match with the current request for
    // 'requested.com'.
    auto remove_function =
        base::MakeRefCounted<PermissionsRemoveSiteAccessRequestFunction>();
    remove_function->set_extension(extension.get());
    std::string error = api_test_utils::RunFunctionAndReturnError(
        remove_function.get(), GetFunctionParams(tab_id, "*://*.other.com/*"),
        profile(), api_test_utils::FunctionMode::kNone);
    EXPECT_EQ(
        "Extension cannot remove a site access request that doesn't exist.",
        error);

    // Verify request wasn't removed.
    EXPECT_TRUE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));
  }
}

// Tests extension can remove a site access request that matches an existent
// request.
TEST_F(PermissionsAPISiteAccessRequestsUnitTest,
       RemoveSiteAccessRequest_TabId_Valid) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Extension")
          .SetManifestKey("host_permissions",
                          base::Value::List().Append("*://*.requested.com/*"))
          .Build();
  AddExtensionAndWithheldPermissions(*extension);

  // Open tab on a url requested by the extension.
  NavigateTo("http://www.requested.com");
  int tab_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetActiveWebContents());

  auto* permissions_manager = PermissionsManager::Get(profile());

  // Extension can remove a site access request that matches all patterns (by
  // not specifying one) when it matches the active request (that matches all
  // patterns).
  {
    // Add a site access request without a pattern.
    auto add_function =
        base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
    add_function->set_extension(extension.get());
    EXPECT_TRUE(api_test_utils::RunFunction(
        add_function.get(), GetFunctionParams(tab_id), profile(),
        api_test_utils::FunctionMode::kNone));

    // Verify site access request was added.
    EXPECT_TRUE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));

    // Remove a site access request without a pattern.
    auto remove_function =
        base::MakeRefCounted<PermissionsRemoveSiteAccessRequestFunction>();
    remove_function->set_extension(extension.get());
    EXPECT_TRUE(api_test_utils::RunFunction(
        remove_function.get(), GetFunctionParams(tab_id), profile(),
        api_test_utils::FunctionMode::kNone));

    // Verify request was removed.
    EXPECT_FALSE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));
  }

  // Extension can remove a site access request with a pattern when it matches
  // the current request (with the same pattern).
  {
    // Add a site access request with 'requested.com' pattern.
    auto add_function =
        base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
    add_function->set_extension(extension.get());
    EXPECT_TRUE(api_test_utils::RunFunction(
        add_function.get(),
        GetFunctionParams(tab_id, /*pattern=*/"*://*.requested.com/*"),
        profile(), api_test_utils::FunctionMode::kNone));

    // Verify site access request was added.
    EXPECT_TRUE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));

    // Remove a site access request with 'requested.com' pattern.
    auto remove_function =
        base::MakeRefCounted<PermissionsRemoveSiteAccessRequestFunction>();
    remove_function->set_extension(extension.get());
    EXPECT_TRUE(api_test_utils::RunFunction(
        remove_function.get(), GetFunctionParams(tab_id), profile(),
        api_test_utils::FunctionMode::kNone));

    // Verify request was removed.
    EXPECT_FALSE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));
  }

  // Extension can remove a site access request without a pattern when it
  // matches the active request (with a pattern).
  {
    // Add a site access request with 'requested.com' pattern.
    auto add_function =
        base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
    add_function->set_extension(extension.get());
    EXPECT_TRUE(api_test_utils::RunFunction(
        add_function.get(),
        GetFunctionParams(tab_id, /*pattern=*/"*://*.requested.com/*"),
        profile(), api_test_utils::FunctionMode::kNone));

    // Verify site access request was added.
    EXPECT_TRUE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));

    // Remove a site access request without specifying pattern (which matches to
    // all patterns). Function is valid because it matches the current request
    // ('all patterns' which matches current request on 'requested.com').
    auto remove_function =
        base::MakeRefCounted<PermissionsRemoveSiteAccessRequestFunction>();
    remove_function->set_extension(extension.get());
    EXPECT_TRUE(api_test_utils::RunFunction(
        remove_function.get(), GetFunctionParams(tab_id), profile(),
        api_test_utils::FunctionMode::kNone));

    // Verify request was removed.
    EXPECT_FALSE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));
  }
}

// Tests extension can remove a site access request for a document, if request
// is existent.
// Note: Document id is converted to tab id. Thus, here we only need to test the
// base cases since we have extensive testing for removing requests with tab id.
TEST_F(PermissionsAPISiteAccessRequestsUnitTest,
       RemoveSiteAccessRequest_DocumentId) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Extension")
          .SetManifestKey("host_permissions",
                          base::Value::List().Append("*://*.requested.com/*"))
          .Build();
  AddExtensionAndWithheldPermissions(*extension);

  // Open tab on a url requested by the extension.
  NavigateTo("http://www.requested.com");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  std::string document_id =
      ExtensionApiFrameIdMap::GetDocumentId(web_contents->GetPrimaryMainFrame())
          .ToString();

  auto* permissions_manager = PermissionsManager::Get(profile());
  auto function_params = [](const std::string& document_id) {
    return base::StringPrintf(R"([{"documentId": "%s"}])", document_id.c_str());
  };

  // Remove site access request for document, when it has no active requests.
  {
    auto function =
        base::MakeRefCounted<PermissionsRemoveSiteAccessRequestFunction>();
    function->set_extension(extension.get());

    std::string error = api_test_utils::RunFunctionAndReturnError(
        function.get(), function_params(document_id), profile(),
        api_test_utils::FunctionMode::kNone);
    EXPECT_EQ(
        "Extension cannot remove a site access request that doesn't exist.",
        error);

    // Verify there is no request.
    EXPECT_FALSE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));
  }

  // Add site access request for document.
  {
    auto function =
        base::MakeRefCounted<PermissionsAddSiteAccessRequestFunction>();
    function->set_extension(extension.get());
    EXPECT_TRUE(api_test_utils::RunFunction(
        function.get(), function_params(document_id), profile(),
        api_test_utils::FunctionMode::kNone));

    // Verify site access request was added.
    EXPECT_TRUE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));
  }

  // Remove site access request for document, when it has an active requests.
  {
    auto function =
        base::MakeRefCounted<PermissionsRemoveSiteAccessRequestFunction>();
    function->set_extension(extension.get());

    EXPECT_TRUE(api_test_utils::RunFunction(
        function.get(), function_params(document_id), profile(),
        api_test_utils::FunctionMode::kNone));

    // Verify request was removed.
    EXPECT_FALSE(permissions_manager->HasActiveSiteAccessRequest(
        tab_id, extension->id()));
  }
}

}  // namespace extensions
