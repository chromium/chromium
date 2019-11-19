// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/extension_info_generator.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/developer_private/inspectable_views_finder.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/permissions_test_util.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/pref_names.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/value_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace developer = api::developer_private;

namespace {

const char kAllHostsPermission[] = "*://*/*";

std::unique_ptr<base::DictionaryValue> DeserializeJSONTestData(
    const base::FilePath& path,
    std::string* error) {
  JSONFileValueDeserializer deserializer(path);
  return base::DictionaryValue::From(deserializer.Deserialize(nullptr, error));
}

// Returns a pointer to the ExtensionInfo for an extension with |id| if it
// is present in |list|.
const developer::ExtensionInfo* GetInfoFromList(
    const ExtensionInfoGenerator::ExtensionInfoList& list,
    const std::string& id) {
  for (const auto& item : list) {
    if (item.id == id)
      return &item;
  }
  return nullptr;
}

// Converts the SiteControls hosts list to a JSON string. This makes test
// validation considerably more concise and readable.
std::string SiteControlsToString(
    const std::vector<developer::SiteControl>& controls) {
  base::Value list(base::Value::Type::LIST);
  list.GetList().reserve(controls.size());
  for (const auto& control : controls) {
    std::unique_ptr<base::Value> control_value = control.ToValue();
    list.Append(std::move(*control_value));
  }

  std::string json;
  CHECK(base::JSONWriter::Write(list, &json));
  return json;
}

}  // namespace

class ExtensionInfoGeneratorUnitTest : public ExtensionServiceTestBase {
 public:
  ExtensionInfoGeneratorUnitTest() {}
  ~ExtensionInfoGeneratorUnitTest() override {}

 protected:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
  }

  void OnInfoGenerated(std::unique_ptr<developer::ExtensionInfo>* info_out,
                       ExtensionInfoGenerator::ExtensionInfoList list) {
    EXPECT_EQ(1u, list.size());
    if (!list.empty())
      info_out->reset(new developer::ExtensionInfo(std::move(list[0])));
    std::move(quit_closure_).Run();
  }

  std::unique_ptr<developer::ExtensionInfo> GenerateExtensionInfo(
      const std::string& extension_id) {
    std::unique_ptr<developer::ExtensionInfo> info;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    std::unique_ptr<ExtensionInfoGenerator> generator(
        new ExtensionInfoGenerator(browser_context()));
    generator->CreateExtensionInfo(
        extension_id,
        base::Bind(&ExtensionInfoGeneratorUnitTest::OnInfoGenerated,
                   base::Unretained(this), base::Unretained(&info)));
    run_loop.Run();
    return info;
  }

  void OnInfosGenerated(ExtensionInfoGenerator::ExtensionInfoList* out,
                        ExtensionInfoGenerator::ExtensionInfoList list) {
    *out = std::move(list);
    std::move(quit_closure_).Run();
  }

  ExtensionInfoGenerator::ExtensionInfoList GenerateExtensionsInfo() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    ExtensionInfoGenerator generator(browser_context());
    ExtensionInfoGenerator::ExtensionInfoList result;
    generator.CreateExtensionsInfo(
        true, /* include_disabled */
        true, /* include_terminated */
        base::Bind(&ExtensionInfoGeneratorUnitTest::OnInfosGenerated,
                   base::Unretained(this), base::Unretained(&result)));
    run_loop.Run();
    return result;
  }

  const scoped_refptr<const Extension> CreateExtension(
      const std::string& name,
      std::unique_ptr<base::ListValue> permissions,
      Manifest::Location location) {
    const std::string kId = crx_file::id_util::GenerateId(name);
    scoped_refptr<const Extension> extension =
        ExtensionBuilder()
            .SetManifest(DictionaryBuilder()
                             .Set("name", name)
                             .Set("description", "an extension")
                             .Set("manifest_version", 2)
                             .Set("version", "1.0.0")
                             .Set("permissions", std::move(permissions))
                             .Build())
            .SetLocation(location)
            .SetID(kId)
            .Build();

    ExtensionRegistry::Get(profile())->AddEnabled(extension);
    PermissionsUpdater updater(profile());
    updater.InitializePermissions(extension.get());
    updater.GrantActivePermissions(extension.get());

    return extension;
  }

  std::unique_ptr<developer::ExtensionInfo> CreateExtensionInfoFromPath(
      const base::FilePath& extension_path,
      Manifest::Location location) {
    ChromeTestExtensionLoader loader(browser_context());
    loader.set_location(location);
    loader.set_creation_flags(Extension::REQUIRE_KEY);
    scoped_refptr<const Extension> extension =
        loader.LoadExtension(extension_path);
    CHECK(extension.get());

    return GenerateExtensionInfo(extension->id());
  }

  void CompareExpectedAndActualOutput(
      const base::FilePath& extension_path,
      InspectableViewsFinder::ViewList views,
      const base::FilePath& expected_output_path) {
    std::string error;
    std::unique_ptr<base::DictionaryValue> expected_output_data(
        DeserializeJSONTestData(expected_output_path, &error));
    EXPECT_EQ(std::string(), error);

    // Produce test output.
    std::unique_ptr<developer::ExtensionInfo> info =
        CreateExtensionInfoFromPath(extension_path, Manifest::UNPACKED);
    info->views = std::move(views);
    std::unique_ptr<base::DictionaryValue> actual_output_data = info->ToValue();
    ASSERT_TRUE(actual_output_data);

    // Compare the outputs.
    // Ignore unknown fields in the actual output data.
    std::string paths_details = " - expected (" +
        expected_output_path.MaybeAsASCII() + ") vs. actual (" +
        extension_path.MaybeAsASCII() + ")";
    std::string expected_string;
    std::string actual_string;
    for (base::DictionaryValue::Iterator field(*expected_output_data);
         !field.IsAtEnd(); field.Advance()) {
      const base::Value& expected_value = field.value();
      base::Value* actual_value = nullptr;
      EXPECT_TRUE(actual_output_data->Get(field.key(), &actual_value)) <<
          field.key() + " is missing" + paths_details;
      if (!actual_value)
        continue;
      if (!actual_value->Equals(&expected_value)) {
        base::JSONWriter::Write(expected_value, &expected_string);
        base::JSONWriter::Write(*actual_value, &actual_string);
        EXPECT_EQ(expected_string, actual_string) <<
            field.key() << paths_details;
      }
    }
  }

 private:
  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInfoGeneratorUnitTest);
};

// Test some of the basic fields.
TEST_F(ExtensionInfoGeneratorUnitTest, BasicInfoTest) {
  // Enable error console for testing.
  FeatureSwitch::ScopedOverride error_console_override(
      FeatureSwitch::error_console(), true);
  profile()->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);

  const char kName[] = "extension name";
  const char kVersion[] = "1.0.0.1";
  std::string id = crx_file::id_util::GenerateId("alpha");
  std::unique_ptr<base::DictionaryValue> manifest =
      DictionaryBuilder()
          .Set("name", kName)
          .Set("version", kVersion)
          .Set("manifest_version", 2)
          .Set("description", "an extension")
          .Set("permissions", ListBuilder()
                                  .Append("file://*/*")
                                  .Append("tabs")
                                  .Append("*://*.google.com/*")
                                  .Append("*://*.example.com/*")
                                  .Append("*://*.foo.bar/*")
                                  .Append("*://*.chromium.org/*")
                                  .Build())
          .Build();
  std::unique_ptr<base::DictionaryValue> manifest_copy(manifest->DeepCopy());
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetLocation(Manifest::UNPACKED)
          .SetPath(data_dir())
          .SetID(id)
          .Build();
  service()->AddExtension(extension.get());
  ErrorConsole* error_console = ErrorConsole::Get(profile());
  const GURL kContextUrl("http://example.com");
  error_console->ReportError(std::make_unique<RuntimeError>(
      extension->id(), false, base::UTF8ToUTF16("source"),
      base::UTF8ToUTF16("message"),
      StackTrace(1, StackFrame(1, 1, base::UTF8ToUTF16("source"),
                               base::UTF8ToUTF16("function"))),
      kContextUrl, logging::LOG_ERROR, 1, 1));
  error_console->ReportError(std::make_unique<ManifestError>(
      extension->id(), base::UTF8ToUTF16("message"), base::UTF8ToUTF16("key"),
      base::string16()));
  error_console->ReportError(std::make_unique<RuntimeError>(
      extension->id(), false, base::UTF8ToUTF16("source"),
      base::UTF8ToUTF16("message"),
      StackTrace(1, StackFrame(1, 1, base::UTF8ToUTF16("source"),
                               base::UTF8ToUTF16("function"))),
      kContextUrl, logging::LOG_WARNING, 1, 1));

  // It's not feasible to validate every field here, because that would be
  // a duplication of the logic in the method itself. Instead, test a handful
  // of fields for sanity.
  std::unique_ptr<api::developer_private::ExtensionInfo> info =
      GenerateExtensionInfo(extension->id());
  ASSERT_TRUE(info.get());
  EXPECT_EQ(kName, info->name);
  EXPECT_EQ(id, info->id);
  EXPECT_EQ(kVersion, info->version);
  EXPECT_EQ(info->location, developer::LOCATION_UNPACKED);
  ASSERT_TRUE(info->path);
  EXPECT_EQ(data_dir(), base::FilePath::FromUTF8Unsafe(*info->path));
  EXPECT_EQ(api::developer_private::EXTENSION_STATE_ENABLED, info->state);
  EXPECT_EQ(api::developer_private::EXTENSION_TYPE_EXTENSION, info->type);
  EXPECT_TRUE(info->file_access.is_enabled);
  EXPECT_FALSE(info->file_access.is_active);
  EXPECT_TRUE(info->incognito_access.is_enabled);
  EXPECT_FALSE(info->incognito_access.is_active);

  // Strip out the kHostReadWrite permission created by the extension requesting
  // host permissions above; runtime host permissions mean these are always
  // present but not necessarily operative. There should only be one entry,
  // though. This is necessary because the code below wants to assert that every
  // entry in |messages| has a matching entry in
  // |info->permissions.simple_permissions|, and kHostReadWrite is not a simple
  // permission.
  PermissionMessages messages;
  for (const PermissionMessage& message :
       extension->permissions_data()->GetPermissionMessages()) {
    if (!message.permissions().ContainsID(
            extensions::APIPermission::kHostReadWrite)) {
      messages.push_back(message);
    }
  }

  ASSERT_EQ(messages.size(), info->permissions.simple_permissions.size());
  size_t i = 0;
  for (const PermissionMessage& message : messages) {
    const api::developer_private::Permission& info_permission =
        info->permissions.simple_permissions[i];
    EXPECT_EQ(message.message(), base::UTF8ToUTF16(info_permission.message));
    const std::vector<base::string16>& submessages = message.submessages();
    ASSERT_EQ(submessages.size(), info_permission.submessages.size());
    for (size_t j = 0; j < submessages.size(); ++j) {
      EXPECT_EQ(submessages[j],
                base::UTF8ToUTF16(info_permission.submessages[j]));
    }
    ++i;
  }
  EXPECT_TRUE(info->permissions.runtime_host_permissions);

  ASSERT_EQ(2u, info->runtime_errors.size());
  const api::developer_private::RuntimeError& runtime_error =
      info->runtime_errors[0];
  EXPECT_EQ(extension->id(), runtime_error.extension_id);
  EXPECT_EQ(api::developer_private::ERROR_TYPE_RUNTIME, runtime_error.type);
  EXPECT_EQ(api::developer_private::ERROR_LEVEL_ERROR,
            runtime_error.severity);
  EXPECT_EQ(kContextUrl, GURL(runtime_error.context_url));
  EXPECT_EQ(1u, runtime_error.stack_trace.size());
  ASSERT_EQ(1u, info->manifest_errors.size());
  const api::developer_private::RuntimeError& runtime_error_verbose =
      info->runtime_errors[1];
  EXPECT_EQ(api::developer_private::ERROR_LEVEL_WARN,
            runtime_error_verbose.severity);
  const api::developer_private::ManifestError& manifest_error =
      info->manifest_errors[0];
  EXPECT_EQ(extension->id(), manifest_error.extension_id);

  // Test an extension that isn't unpacked.
  manifest_copy->SetString("update_url",
                           "https://clients2.google.com/service/update2/crx");
  id = crx_file::id_util::GenerateId("beta");
  extension = ExtensionBuilder()
                  .SetManifest(std::move(manifest_copy))
                  .SetLocation(Manifest::EXTERNAL_PREF)
                  .SetID(id)
                  .Build();
  service()->AddExtension(extension.get());
  info = GenerateExtensionInfo(extension->id());
  EXPECT_EQ(developer::LOCATION_THIRD_PARTY, info->location);
  EXPECT_FALSE(info->path);
}

// Test three generated json outputs.
TEST_F(ExtensionInfoGeneratorUnitTest, GenerateExtensionsJSONData) {
  // Test Extension1
  base::FilePath extension_path =
      data_dir().AppendASCII("good")
                .AppendASCII("Extensions")
                .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                .AppendASCII("1.0.0.0");

  base::FilePath expected_outputs_path =
      data_dir().AppendASCII("api_test")
                .AppendASCII("developer")
                .AppendASCII("generated_output");

  {
    InspectableViewsFinder::ViewList views;
    views.push_back(InspectableViewsFinder::ConstructView(
        GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/bar.html"),
        42, 88, true, false, VIEW_TYPE_TAB_CONTENTS));
    views.push_back(InspectableViewsFinder::ConstructView(
        GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/dog.html"), 0,
        0, false, true, VIEW_TYPE_TAB_CONTENTS));

    CompareExpectedAndActualOutput(
        extension_path, std::move(views),
        expected_outputs_path.AppendASCII(
            "behllobkkfkfnphdnhnkndlbkcpglgmj.json"));
  }

#if !defined(OS_CHROMEOS)
  // Test Extension2
  extension_path = data_dir().AppendASCII("good")
                             .AppendASCII("Extensions")
                             .AppendASCII("hpiknbiabeeppbpihjehijgoemciehgk")
                             .AppendASCII("2");

  {
    // It's OK to have duplicate URLs, so long as the IDs are different.
    InspectableViewsFinder::ViewList views;
    views.push_back(InspectableViewsFinder::ConstructView(
        GURL("chrome-extension://hpiknbiabeeppbpihjehijgoemciehgk/bar.html"),
        42, 88, true, false, VIEW_TYPE_TAB_CONTENTS));
    views.push_back(InspectableViewsFinder::ConstructView(
        GURL("chrome-extension://hpiknbiabeeppbpihjehijgoemciehgk/bar.html"), 0,
        0, false, true, VIEW_TYPE_TAB_CONTENTS));

    CompareExpectedAndActualOutput(
        extension_path, std::move(views),
        expected_outputs_path.AppendASCII(
            "hpiknbiabeeppbpihjehijgoemciehgk.json"));
  }
#endif

  // Test Extension3
  extension_path = data_dir().AppendASCII("good")
                             .AppendASCII("Extensions")
                             .AppendASCII("bjafgdebaacbbbecmhlhpofkepfkgcpa")
                             .AppendASCII("1.0");
  CompareExpectedAndActualOutput(extension_path,
                                 InspectableViewsFinder::ViewList(),
                                 expected_outputs_path.AppendASCII(
                                     "bjafgdebaacbbbecmhlhpofkepfkgcpa.json"));
}

// Tests the generation of the runtime host permissions entries.
TEST_F(ExtensionInfoGeneratorUnitTest, RuntimeHostPermissions) {
  scoped_refptr<const Extension> all_urls_extension = CreateExtension(
      "all_urls", ListBuilder().Append(kAllHostsPermission).Build(),
      Manifest::INTERNAL);

  std::unique_ptr<developer::ExtensionInfo> info =
      GenerateExtensionInfo(all_urls_extension->id());

  // The extension should be set to run on all sites.
  ASSERT_TRUE(info->permissions.runtime_host_permissions);
  const developer::RuntimeHostPermissions* runtime_hosts =
      info->permissions.runtime_host_permissions.get();
  EXPECT_EQ(developer::HOST_ACCESS_ON_ALL_SITES, runtime_hosts->host_access);
  EXPECT_EQ(R"([{"granted":true,"host":"*://*/*"}])",
            SiteControlsToString(runtime_hosts->hosts));
  EXPECT_TRUE(runtime_hosts->has_all_hosts);
  // With runtime host permissions, no host permissions are added to
  // |simple_permissions|.
  EXPECT_THAT(info->permissions.simple_permissions, testing::IsEmpty());

  // Withholding host permissions should result in the extension being set to
  // run on click.
  ScriptingPermissionsModifier permissions_modifier(profile(),
                                                    all_urls_extension);
  permissions_modifier.SetWithholdHostPermissions(true);
  info = GenerateExtensionInfo(all_urls_extension->id());
  runtime_hosts = info->permissions.runtime_host_permissions.get();
  EXPECT_EQ(developer::HOST_ACCESS_ON_CLICK, runtime_hosts->host_access);
  EXPECT_EQ(R"([{"granted":false,"host":"*://*/*"}])",
            SiteControlsToString(runtime_hosts->hosts));
  EXPECT_TRUE(runtime_hosts->has_all_hosts);
  EXPECT_THAT(info->permissions.simple_permissions, testing::IsEmpty());

  // Granting a host permission should set the extension to run on specific
  // sites, and those sites should be in the specific_site_controls.hosts set.
  permissions_modifier.GrantHostPermission(GURL("https://example.com"));
  info = GenerateExtensionInfo(all_urls_extension->id());
  runtime_hosts = info->permissions.runtime_host_permissions.get();
  EXPECT_EQ(developer::HOST_ACCESS_ON_SPECIFIC_SITES,
            runtime_hosts->host_access);
  EXPECT_EQ(
      R"([{"granted":true,"host":"https://example.com/*"},)"
      R"({"granted":false,"host":"*://*/*"}])",
      SiteControlsToString(runtime_hosts->hosts));
  EXPECT_TRUE(runtime_hosts->has_all_hosts);
  EXPECT_THAT(info->permissions.simple_permissions, testing::IsEmpty());

  // An extension that doesn't request any host permissions should not have
  // runtime access controls.
  scoped_refptr<const Extension> no_urls_extension =
      CreateExtension("no urls", ListBuilder().Build(), Manifest::INTERNAL);
  info = GenerateExtensionInfo(no_urls_extension->id());
  EXPECT_FALSE(info->permissions.runtime_host_permissions);
}

// Tests that specific_site_controls is correctly populated when permissions
// are granted by the user beyond what the extension originally requested in the
// manifest.
TEST_F(ExtensionInfoGeneratorUnitTest,
       RuntimeHostPermissionsBeyondRequestedScope) {
  scoped_refptr<const Extension> extension =
      CreateExtension("extension", ListBuilder().Append("http://*/*").Build(),
                      Manifest::INTERNAL);

  std::unique_ptr<developer::ExtensionInfo> info =
      GenerateExtensionInfo(extension->id());

  // Withhold permissions, and grant *://chromium.org/*.
  ScriptingPermissionsModifier permissions_modifier(profile(), extension);
  permissions_modifier.SetWithholdHostPermissions(true);
  URLPattern all_chromium(Extension::kValidHostPermissionSchemes,
                          "*://chromium.org/*");
  PermissionSet all_chromium_set(APIPermissionSet(), ManifestPermissionSet(),
                                 URLPatternSet({all_chromium}),
                                 URLPatternSet({all_chromium}));
  permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
      profile(), *extension, all_chromium_set);

  // The extension should only be granted http://chromium.org/* (since that's
  // the intersection with what it requested).
  URLPattern http_chromium(Extension::kValidHostPermissionSchemes,
                           "http://chromium.org/*");
  EXPECT_EQ(PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                          URLPatternSet({http_chromium}), URLPatternSet()),
            extension->permissions_data()->active_permissions());

  // The generated info should use the entirety of the granted permission,
  // which is *://chromium.org/*.
  info = GenerateExtensionInfo(extension->id());
  ASSERT_TRUE(info->permissions.runtime_host_permissions);
  const developer::RuntimeHostPermissions* runtime_hosts =
      info->permissions.runtime_host_permissions.get();
  EXPECT_EQ(developer::HOST_ACCESS_ON_SPECIFIC_SITES,
            runtime_hosts->host_access);
  EXPECT_EQ(
      R"([{"granted":true,"host":"*://chromium.org/*"},)"
      R"({"granted":false,"host":"http://*/*"}])",
      SiteControlsToString(runtime_hosts->hosts));
  EXPECT_TRUE(runtime_hosts->has_all_hosts);
}

// Tests that specific_site_controls is correctly populated when the extension
// requests access to specific hosts.
TEST_F(ExtensionInfoGeneratorUnitTest, RuntimeHostPermissionsSpecificHosts) {
  scoped_refptr<const Extension> extension =
      CreateExtension("extension",
                      ListBuilder()
                          .Append("https://example.com/*")
                          .Append("https://chromium.org/*")
                          .Build(),
                      Manifest::INTERNAL);

  std::unique_ptr<developer::ExtensionInfo> info =
      GenerateExtensionInfo(extension->id());

  // Withhold permissions, and grant *://chromium.org/*.
  ScriptingPermissionsModifier permissions_modifier(profile(), extension);
  permissions_modifier.SetWithholdHostPermissions(true);
  URLPattern all_chromium(Extension::kValidHostPermissionSchemes,
                          "https://chromium.org/*");
  PermissionSet all_chromium_set(APIPermissionSet(), ManifestPermissionSet(),
                                 URLPatternSet({all_chromium}),
                                 URLPatternSet({all_chromium}));
  permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
      profile(), *extension, all_chromium_set);

  // The generated info should use the entirety of the granted permission,
  // which is *://chromium.org/*.
  info = GenerateExtensionInfo(extension->id());
  ASSERT_TRUE(info->permissions.runtime_host_permissions);
  const developer::RuntimeHostPermissions* runtime_hosts =
      info->permissions.runtime_host_permissions.get();
  EXPECT_EQ(developer::HOST_ACCESS_ON_SPECIFIC_SITES,
            runtime_hosts->host_access);
  EXPECT_EQ(
      R"([{"granted":true,"host":"https://chromium.org/*"},)"
      R"({"granted":false,"host":"https://example.com/*"}])",
      SiteControlsToString(runtime_hosts->hosts));
  EXPECT_FALSE(runtime_hosts->has_all_hosts);
}

// Tests the population of withheld runtime hosts when they overlap with granted
// patterns.
TEST_F(ExtensionInfoGeneratorUnitTest, WithheldUrlsOverlapping) {
  scoped_refptr<const Extension> extension =
      CreateExtension("extension",
                      ListBuilder()
                          .Append("*://example.com/*")
                          .Append("https://chromium.org/*")
                          .Build(),
                      Manifest::INTERNAL);
  ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);

  {
    std::unique_ptr<developer::ExtensionInfo> info =
        GenerateExtensionInfo(extension->id());
    ASSERT_TRUE(info->permissions.runtime_host_permissions);
    // Initially, no hosts are granted.
    EXPECT_EQ(
        R"([{"granted":false,"host":"*://example.com/*"},)"
        R"({"granted":false,"host":"https://chromium.org/*"}])",
        SiteControlsToString(
            info->permissions.runtime_host_permissions->hosts));
    EXPECT_FALSE(info->permissions.runtime_host_permissions->has_all_hosts);
    EXPECT_EQ(developer::HOST_ACCESS_ON_CLICK,
              info->permissions.runtime_host_permissions->host_access);
  }

  // Grant http://example.com, which is a subset of the requested host pattern
  // (*://example.com).
  modifier.GrantHostPermission(GURL("http://example.com/"));
  {
    std::unique_ptr<developer::ExtensionInfo> info =
        GenerateExtensionInfo(extension->id());
    ASSERT_TRUE(info->permissions.runtime_host_permissions);
    // We should display that http://example.com is granted, but *://example.com
    // is still requested. This is technically correct.
    // TODO(devlin): This is an edge case, so it's okay for it to be a little
    // rough (as long as it's not incorrect), but it would be nice to polish it
    // out. Ideally, for extensions requesting specific hosts, we'd only allow
    // granting/revoking specific patterns (e.g., all example.com sites).
    EXPECT_EQ(
        R"([{"granted":true,"host":"http://example.com/*"},)"
        R"({"granted":false,"host":"*://example.com/*"},)"
        R"({"granted":false,"host":"https://chromium.org/*"}])",
        SiteControlsToString(
            info->permissions.runtime_host_permissions->hosts));
    EXPECT_FALSE(info->permissions.runtime_host_permissions->has_all_hosts);
    EXPECT_EQ(developer::HOST_ACCESS_ON_SPECIFIC_SITES,
              info->permissions.runtime_host_permissions->host_access);
  }

  // Grant the requested pattern ("*://example.com/*").
  {
    URLPattern example_com(Extension::kValidHostPermissionSchemes,
                           "*://example.com/*");
    PermissionSet example_com_set(APIPermissionSet(), ManifestPermissionSet(),
                                  URLPatternSet({example_com}),
                                  URLPatternSet({example_com}));
    PermissionsUpdater(profile()).GrantRuntimePermissions(
        *extension, example_com_set, base::DoNothing::Once());
  }

  {
    std::unique_ptr<developer::ExtensionInfo> info =
        GenerateExtensionInfo(extension->id());
    ASSERT_TRUE(info->permissions.runtime_host_permissions);
    // The http://example.com/* pattern should be omitted, since it's consumed
    // by the *://example.com/* pattern.
    EXPECT_EQ(
        R"([{"granted":true,"host":"*://example.com/*"},)"
        R"({"granted":false,"host":"https://chromium.org/*"}])",
        SiteControlsToString(
            info->permissions.runtime_host_permissions->hosts));
    EXPECT_FALSE(info->permissions.runtime_host_permissions->has_all_hosts);
    EXPECT_EQ(developer::HOST_ACCESS_ON_SPECIFIC_SITES,
              info->permissions.runtime_host_permissions->host_access);
  }

  // Grant permission beyond what was requested (*://*.example.com, when
  // subdomains weren't in the extension manifest).
  {
    URLPattern example_com(Extension::kValidHostPermissionSchemes,
                           "*://*.example.com/*");
    PermissionSet example_com_set(APIPermissionSet(), ManifestPermissionSet(),
                                  URLPatternSet({example_com}),
                                  URLPatternSet({example_com}));
    PermissionsUpdater(profile()).GrantRuntimePermissions(
        *extension, example_com_set, base::DoNothing::Once());
  }

  {
    std::unique_ptr<developer::ExtensionInfo> info =
        GenerateExtensionInfo(extension->id());
    ASSERT_TRUE(info->permissions.runtime_host_permissions);
    // The full granted pattern should be visible.
    EXPECT_EQ(
        R"([{"granted":true,"host":"*://*.example.com/*"},)"
        R"({"granted":false,"host":"https://chromium.org/*"}])",
        SiteControlsToString(
            info->permissions.runtime_host_permissions->hosts));
    EXPECT_FALSE(info->permissions.runtime_host_permissions->has_all_hosts);
    EXPECT_EQ(developer::HOST_ACCESS_ON_SPECIFIC_SITES,
              info->permissions.runtime_host_permissions->host_access);
  }
}

// Tests the population of withheld runtime hosts when they overlap with granted
// patterns.
TEST_F(ExtensionInfoGeneratorUnitTest,
       WithheldUrlsOverlappingWithContentScript) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("extension")
          .AddPermissions({"*://example.com/*", "*://chromium.org/*"})
          .AddContentScript("script.js", {"*://example.com/foo"})
          .Build();
  {
    ExtensionRegistry::Get(profile())->AddEnabled(extension);
    PermissionsUpdater updater(profile());
    updater.InitializePermissions(extension.get());
    updater.GrantActivePermissions(extension.get());
  }

  ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);

  {
    std::unique_ptr<developer::ExtensionInfo> info =
        GenerateExtensionInfo(extension->id());
    ASSERT_TRUE(info->permissions.runtime_host_permissions);
    // Initially, no hosts are granted.
    EXPECT_EQ(
        R"([{"granted":false,"host":"*://chromium.org/*"},)"
        R"({"granted":false,"host":"*://example.com/*"}])",
        SiteControlsToString(
            info->permissions.runtime_host_permissions->hosts));
    EXPECT_FALSE(info->permissions.runtime_host_permissions->has_all_hosts);
    EXPECT_EQ(developer::HOST_ACCESS_ON_CLICK,
              info->permissions.runtime_host_permissions->host_access);
  }
}

// Test that file:// access checkbox does not show up when the user can't
// modify an extension's settings. https://crbug.com/173640.
TEST_F(ExtensionInfoGeneratorUnitTest, ExtensionInfoLockedAllUrls) {
  // Force installed extensions aren't user modifyable.
  scoped_refptr<const Extension> locked_extension =
      CreateExtension("locked", ListBuilder().Append("file://*/*").Build(),
                      Manifest::EXTERNAL_POLICY_DOWNLOAD);

  std::unique_ptr<developer::ExtensionInfo> info =
      GenerateExtensionInfo(locked_extension->id());

  // Extension wants file:// access but the checkbox will not appear
  // in chrome://extensions.
  EXPECT_TRUE(locked_extension->wants_file_access());
  EXPECT_FALSE(info->file_access.is_enabled);
  EXPECT_FALSE(info->file_access.is_active);
}

// Tests that file:// access checkbox shows up for extensions with activeTab
// permission. See crbug.com/850643.
TEST_F(ExtensionInfoGeneratorUnitTest, ActiveTabFileUrls) {
  scoped_refptr<const Extension> extension =
      CreateExtension("activeTab", ListBuilder().Append("activeTab").Build(),
                      Manifest::INTERNAL);
  std::unique_ptr<developer::ExtensionInfo> info =
      GenerateExtensionInfo(extension->id());

  EXPECT_TRUE(extension->wants_file_access());
  EXPECT_TRUE(info->file_access.is_enabled);
  EXPECT_FALSE(info->file_access.is_active);
}

// Tests that blacklisted extensions are returned by the ExtensionInfoGenerator.
TEST_F(ExtensionInfoGeneratorUnitTest, Blacklisted) {
  const scoped_refptr<const Extension> extension1 = CreateExtension(
      "test1", std::make_unique<base::ListValue>(), Manifest::INTERNAL);
  const scoped_refptr<const Extension> extension2 = CreateExtension(
      "test2", std::make_unique<base::ListValue>(), Manifest::INTERNAL);

  std::string id1 = extension1->id();
  std::string id2 = extension2->id();
  ASSERT_NE(id1, id2);

  ExtensionInfoGenerator::ExtensionInfoList info_list =
      GenerateExtensionsInfo();
  const developer::ExtensionInfo* info1 = GetInfoFromList(info_list, id1);
  const developer::ExtensionInfo* info2 = GetInfoFromList(info_list, id2);
  ASSERT_NE(nullptr, info1);
  ASSERT_NE(nullptr, info2);
  EXPECT_EQ(developer::EXTENSION_STATE_ENABLED, info1->state);
  EXPECT_EQ(developer::EXTENSION_STATE_ENABLED, info2->state);

  service()->BlacklistExtensionForTest(id1);

  info_list = GenerateExtensionsInfo();
  info1 = GetInfoFromList(info_list, id1);
  info2 = GetInfoFromList(info_list, id2);
  ASSERT_NE(nullptr, info1);
  ASSERT_NE(nullptr, info2);
  EXPECT_EQ(developer::EXTENSION_STATE_BLACKLISTED, info1->state);
  EXPECT_EQ(developer::EXTENSION_STATE_ENABLED, info2->state);
}

}  // namespace extensions
