// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/permissions_test_util.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_error_test_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/install/extension_install_ui.h"
#include "extensions/browser/mock_external_provider.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/test_extension_dir.h"
#include "services/data_decoder/data_decoder_service.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"

namespace extensions {

namespace {

const char kGoodCrx[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
constexpr char kInvalidHost[] = "invalid host";
constexpr char kInvalidHostError[] = "Invalid host.";

std::unique_ptr<KeyedService> BuildAPI(content::BrowserContext* context) {
  return std::make_unique<DeveloperPrivateAPI>(context);
}

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* profile) {
  return std::make_unique<EventRouter>(profile, ExtensionPrefs::Get(profile));
}

bool HasPrefsPermission(bool (*has_pref)(const std::string&,
                                         content::BrowserContext*),
                        content::BrowserContext* context,
                        const std::string& id) {
  return has_pref(id, context);
}

bool WasPermissionsUpdatedEventDispatched(
    const TestEventRouterObserver& observer,
    const ExtensionId& extension_id) {
  const std::string kEventName =
      api::developer_private::OnItemStateChanged::kEventName;
  const auto& event_map = observer.events();
  auto iter = event_map.find(kEventName);
  if (iter == event_map.end())
    return false;

  const Event& event = *iter->second;
  CHECK(event.event_args);
  CHECK_GE(1u, event.event_args->GetList().size());
  std::unique_ptr<api::developer_private::EventData> event_data =
      api::developer_private::EventData::FromValue(
          event.event_args->GetList()[0]);
  if (!event_data)
    return false;

  if (event_data->item_id != extension_id ||
      event_data->event_type !=
          api::developer_private::EVENT_TYPE_PERMISSIONS_CHANGED) {
    return false;
  }

  return true;
}

}  // namespace

class DeveloperPrivateApiUnitTest : public ExtensionServiceTestWithInstall {
 protected:
  DeveloperPrivateApiUnitTest() {}
  ~DeveloperPrivateApiUnitTest() override {}

  void AddMockExternalProvider(
      std::unique_ptr<ExternalProviderInterface> provider) {
    service()->AddProviderForTesting(std::move(provider));
  }

  // A wrapper around extension_function_test_utils::RunFunction that runs with
  // the associated browser, no flags, and can take stack-allocated arguments.
  bool RunFunction(const scoped_refptr<ExtensionFunction>& function,
                   const base::ListValue& args);

  // Loads an unpacked extension that is backed by a real directory, allowing
  // it to be reloaded.
  const Extension* LoadUnpackedExtension();

  // Loads an extension with no real directory; this is faster, but means the
  // extension can't be reloaded.
  const Extension* LoadSimpleExtension();

  // Tests modifying the extension's configuration.
  void TestExtensionPrefSetting(const base::Callback<bool()>& has_pref,
                                const std::string& key,
                                const std::string& extension_id);

  testing::AssertionResult TestPackExtensionFunction(
      const base::ListValue& args,
      api::developer_private::PackStatus expected_status,
      int expected_flags);

  // Execute the updateProfileConfiguration API call with a specified
  // dev_mode. This is done from the webui when the user checks the
  // "Developer Mode" checkbox.
  void UpdateProfileConfigurationDevMode(bool dev_mode);

  // Execute the getProfileConfiguration API and parse its result into a
  // ProfileInfo structure for further verification in the calling test.
  // Will reset the profile_info unique_ptr.
  // Uses ASSERT_* inside - callers should use ASSERT_NO_FATAL_FAILURE.
  void GetProfileConfiguration(
      std::unique_ptr<api::developer_private::ProfileInfo>* profile_info);

  // Runs the API function to update host access for the given |extension| to
  // |new_access|.
  void RunUpdateHostAccess(const Extension& extension,
                           base::StringPiece new_access);

  virtual bool ProfileIsSupervised() const { return false; }

  Browser* browser() { return browser_.get(); }

 private:
  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

  // The browser (and accompanying window).
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;

  std::vector<std::unique_ptr<TestExtensionDir>> test_extension_dirs_;
  policy::MockConfigurationPolicyProvider mock_policy_provider_;

  DISALLOW_COPY_AND_ASSIGN(DeveloperPrivateApiUnitTest);
};

bool DeveloperPrivateApiUnitTest::RunFunction(
    const scoped_refptr<ExtensionFunction>& function,
    const base::ListValue& args) {
  return extension_function_test_utils::RunFunction(
      function.get(), args.CreateDeepCopy(), browser(), api_test_utils::NONE);
}

const Extension* DeveloperPrivateApiUnitTest::LoadUnpackedExtension() {
  constexpr char kManifest[] =
      R"({
           "name": "foo",
           "version": "1.0",
           "manifest_version": 2,
           "permissions": ["*://*/*"]
         })";

  test_extension_dirs_.push_back(std::make_unique<TestExtensionDir>());
  TestExtensionDir* dir = test_extension_dirs_.back().get();
  dir->WriteManifest(kManifest);

  ChromeTestExtensionLoader loader(profile());
  // The fact that unpacked extensions get file access by default is an
  // irrelevant detail to these tests. Disable it.
  loader.set_allow_file_access(false);

  return loader.LoadExtension(dir->UnpackedPath()).get();
}

const Extension* DeveloperPrivateApiUnitTest::LoadSimpleExtension() {
  const char kName[] = "extension name";
  const char kVersion[] = "1.0.0.1";
  std::string id = crx_file::id_util::GenerateId(kName);
  DictionaryBuilder manifest;
  manifest.Set("name", kName)
          .Set("version", kVersion)
          .Set("manifest_version", 2)
          .Set("description", "an extension");
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(manifest.Build())
          .SetLocation(Manifest::INTERNAL)
          .SetID(id)
          .Build();
  service()->AddExtension(extension.get());
  return extension.get();
}

void DeveloperPrivateApiUnitTest::TestExtensionPrefSetting(
    const base::Callback<bool()>& has_pref,
    const std::string& key,
    const std::string& extension_id) {
  scoped_refptr<ExtensionFunction> function(
      new api::DeveloperPrivateUpdateExtensionConfigurationFunction());

  EXPECT_FALSE(has_pref.Run()) << key;

  {
    auto parameters = std::make_unique<base::DictionaryValue>();
    parameters->SetString("extensionId", extension_id);
    parameters->SetBoolean(key, true);

    base::ListValue args;
    args.Append(std::move(parameters));
    EXPECT_FALSE(RunFunction(function, args)) << key;
    EXPECT_EQ("This action requires a user gesture.", function->GetError());

    function = new api::DeveloperPrivateUpdateExtensionConfigurationFunction();
    function->set_source_context_type(Feature::WEBUI_CONTEXT);
    EXPECT_TRUE(RunFunction(function, args)) << key;
    EXPECT_TRUE(has_pref.Run()) << key;

    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
    function = new api::DeveloperPrivateUpdateExtensionConfigurationFunction();
    EXPECT_TRUE(RunFunction(function, args)) << key;
    EXPECT_TRUE(has_pref.Run()) << key;
  }

  {
    auto parameters = std::make_unique<base::DictionaryValue>();
    parameters->SetString("extensionId", extension_id);
    parameters->SetBoolean(key, false);

    base::ListValue args;
    args.Append(std::move(parameters));

    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
    function = new api::DeveloperPrivateUpdateExtensionConfigurationFunction();
    EXPECT_TRUE(RunFunction(function, args)) << key;
    EXPECT_FALSE(has_pref.Run()) << key;
  }
}

testing::AssertionResult DeveloperPrivateApiUnitTest::TestPackExtensionFunction(
    const base::ListValue& args,
    api::developer_private::PackStatus expected_status,
    int expected_flags) {
  scoped_refptr<ExtensionFunction> function(
      new api::DeveloperPrivatePackDirectoryFunction());
  if (!RunFunction(function, args))
    return testing::AssertionFailure() << "Could not run function.";

  // Extract the result. We don't have to test this here, since it's verified as
  // part of the general extension api system.
  const base::Value* response_value = nullptr;
  CHECK(function->GetResultList()->Get(0u, &response_value));
  std::unique_ptr<api::developer_private::PackDirectoryResponse> response =
      api::developer_private::PackDirectoryResponse::FromValue(*response_value);
  CHECK(response);

  if (response->status != expected_status) {
    return testing::AssertionFailure() << "Expected status: " <<
        expected_status << ", found status: " << response->status <<
        ", message: " << response->message;
  }

  if (response->override_flags != expected_flags) {
    return testing::AssertionFailure() << "Expected flags: " <<
        expected_flags << ", found flags: " << response->override_flags;
  }

  return testing::AssertionSuccess();
}

void DeveloperPrivateApiUnitTest::UpdateProfileConfigurationDevMode(
    bool dev_mode) {
  scoped_refptr<ExtensionFunction> function(
      new api::DeveloperPrivateUpdateProfileConfigurationFunction());
  std::unique_ptr<base::ListValue> args =
      ListBuilder()
          .Append(DictionaryBuilder().Set("inDeveloperMode", dev_mode).Build())
          .Build();
  EXPECT_TRUE(RunFunction(function, *args)) << function->GetError();
}

void DeveloperPrivateApiUnitTest::GetProfileConfiguration(
    std::unique_ptr<api::developer_private::ProfileInfo>* profile_info) {
  scoped_refptr<ExtensionFunction> function(
      new api::DeveloperPrivateGetProfileConfigurationFunction());
  base::ListValue args;
  EXPECT_TRUE(RunFunction(function, args)) << function->GetError();

  ASSERT_TRUE(function->GetResultList());
  ASSERT_EQ(1u, function->GetResultList()->GetSize());
  const base::Value* response_value = nullptr;
  function->GetResultList()->Get(0u, &response_value);
  *profile_info =
      api::developer_private::ProfileInfo::FromValue(*response_value);
}

void DeveloperPrivateApiUnitTest::RunUpdateHostAccess(
    const Extension& extension,
    base::StringPiece new_access) {
  SCOPED_TRACE(new_access);
  ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
  scoped_refptr<ExtensionFunction> function = base::MakeRefCounted<
      api::DeveloperPrivateUpdateExtensionConfigurationFunction>();
  std::string args =
      base::StringPrintf(R"([{"extensionId": "%s", "hostAccess": "%s"}])",
                         extension.id().c_str(), new_access.data());
  EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile()))
      << function->GetError();
}

void DeveloperPrivateApiUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();

  // By not specifying a pref_file filepath, we get a
  // sync_preferences::TestingPrefServiceSyncable
  // - see BuildTestingProfile in extension_service_test_base.cc.
  ExtensionServiceInitParams init_params = CreateDefaultInitParams();
  init_params.pref_file.clear();
  init_params.profile_is_supervised = ProfileIsSupervised();
  InitializeExtensionService(init_params);

  browser_window_.reset(new TestBrowserWindow());
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = browser_window_.get();
  browser_.reset(new Browser(params));

  // Allow the API to be created.
  EventRouterFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildEventRouter));

  DeveloperPrivateAPI::GetFactoryInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildAPI));

  // Loading unpacked extensions through the developerPrivate API requires
  // developer mode to be enabled.
  profile()->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);
}

void DeveloperPrivateApiUnitTest::TearDown() {
  test_extension_dirs_.clear();
  browser_.reset();
  browser_window_.reset();
  ExtensionServiceTestBase::TearDown();
}

// Test developerPrivate.updateExtensionConfiguration.
TEST_F(DeveloperPrivateApiUnitTest,
       DeveloperPrivateUpdateExtensionConfiguration) {
  // Sadly, we need a "real" directory here, because toggling prefs causes
  // a reload (which needs a path).
  const Extension* extension = LoadUnpackedExtension();
  const std::string& id = extension->id();

  ScriptingPermissionsModifier(profile(), base::WrapRefCounted(extension))
      .SetWithholdHostPermissions(true);

  TestExtensionPrefSetting(
      base::Bind(&HasPrefsPermission, &util::IsIncognitoEnabled, profile(), id),
      "incognitoAccess", id);
  TestExtensionPrefSetting(
      base::Bind(&HasPrefsPermission, &util::AllowFileAccess, profile(), id),
      "fileAccess", id);
}

// Test developerPrivate.reload.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateReload) {
  const Extension* extension = LoadUnpackedExtension();
  std::string extension_id = extension->id();
  scoped_refptr<ExtensionFunction> function(
      new api::DeveloperPrivateReloadFunction());
  base::ListValue reload_args;
  reload_args.AppendString(extension_id);

  TestExtensionRegistryObserver registry_observer(registry());
  EXPECT_TRUE(RunFunction(function, reload_args));
  scoped_refptr<const Extension> unloaded_extension =
      registry_observer.WaitForExtensionUnloaded();
  EXPECT_EQ(extension, unloaded_extension);
  const Extension* reloaded_extension =
      registry_observer.WaitForExtensionLoaded();
  EXPECT_EQ(extension_id, reloaded_extension->id());
}

// Test developerPrivate.packDirectory.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivatePackFunction) {
  // Use a temp dir isolating the extension dir and its generated files.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath root_path = data_dir().AppendASCII("simple_with_popup");
  ASSERT_TRUE(base::CopyDirectory(root_path, temp_dir.GetPath(), true));

  base::FilePath temp_root_path =
      temp_dir.GetPath().Append(root_path.BaseName());
  base::FilePath crx_path =
      temp_dir.GetPath().AppendASCII("simple_with_popup.crx");
  base::FilePath pem_path =
      temp_dir.GetPath().AppendASCII("simple_with_popup.pem");

  EXPECT_FALSE(base::PathExists(crx_path))
      << "crx should not exist before the test is run!";
  EXPECT_FALSE(base::PathExists(pem_path))
      << "pem should not exist before the test is run!";

  // First, test a directory that should pack properly.
  base::ListValue pack_args;
  pack_args.AppendString(temp_root_path.AsUTF8Unsafe());
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args, api::developer_private::PACK_STATUS_SUCCESS, 0));

  // Should have created crx file and pem file.
  EXPECT_TRUE(base::PathExists(crx_path));
  EXPECT_TRUE(base::PathExists(pem_path));

  // Deliberately don't cleanup the files, and append the pem path.
  pack_args.AppendString(pem_path.AsUTF8Unsafe());

  // Try to pack again - we should get a warning abot overwriting the crx.
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args,
      api::developer_private::PACK_STATUS_WARNING,
      ExtensionCreator::kOverwriteCRX));

  // Try to pack again, with the overwrite flag; this should succeed.
  pack_args.AppendInteger(ExtensionCreator::kOverwriteCRX);
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args, api::developer_private::PACK_STATUS_SUCCESS, 0));

  // Try to pack a final time when omitting (an existing) pem file. We should
  // get an error.
  base::DeleteFile(crx_path, false);
  EXPECT_TRUE(pack_args.Remove(1u, nullptr));  // Remove the pem key argument.
  EXPECT_TRUE(pack_args.Remove(1u, nullptr));  // Remove the flags argument.
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args, api::developer_private::PACK_STATUS_ERROR, 0));
}

// Test developerPrivate.choosePath.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateChoosePath) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  base::FilePath expected_dir_path =
      data_dir().AppendASCII("simple_with_popup");
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&expected_dir_path);

  // Try selecting a directory.
  base::ListValue choose_args;
  choose_args.AppendString("FOLDER");
  choose_args.AppendString("LOAD");
  scoped_refptr<ExtensionFunction> function(
      new api::DeveloperPrivateChoosePathFunction());
  function->SetRenderFrameHost(web_contents->GetMainFrame());
  EXPECT_TRUE(RunFunction(function, choose_args)) << function->GetError();
  std::string path;
  EXPECT_TRUE(function->GetResultList() &&
              function->GetResultList()->GetString(0, &path));
  EXPECT_EQ(path, expected_dir_path.AsUTF8Unsafe());

  // Try selecting a pem file.
  base::FilePath expected_file_path =
      data_dir().AppendASCII("simple_with_popup.pem");
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&expected_file_path);
  choose_args.Clear();
  choose_args.AppendString("FILE");
  choose_args.AppendString("PEM");
  function = new api::DeveloperPrivateChoosePathFunction();
  function->SetRenderFrameHost(web_contents->GetMainFrame());
  EXPECT_TRUE(RunFunction(function, choose_args)) << function->GetError();
  EXPECT_TRUE(function->GetResultList() &&
              function->GetResultList()->GetString(0, &path));
  EXPECT_EQ(path, expected_file_path.AsUTF8Unsafe());

  // Try canceling the file dialog.
  api::EntryPicker::SkipPickerAndAlwaysCancelForTest();
  function = new api::DeveloperPrivateChoosePathFunction();
  function->SetRenderFrameHost(web_contents->GetMainFrame());
  EXPECT_FALSE(RunFunction(function, choose_args));
  EXPECT_EQ(std::string("File selection was canceled."), function->GetError());
}

// Test developerPrivate.loadUnpacked.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateLoadUnpacked) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  base::FilePath path = data_dir().AppendASCII("simple_with_popup");
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&path);

  // Try loading a good extension (it should succeed, and the extension should
  // be added).
  scoped_refptr<ExtensionFunction> function(
      new api::DeveloperPrivateLoadUnpackedFunction());
  function->SetRenderFrameHost(web_contents->GetMainFrame());
  ExtensionIdSet current_ids = registry()->enabled_extensions().GetIDs();
  EXPECT_TRUE(RunFunction(function, base::ListValue())) << function->GetError();
  // We should have added one new extension.
  ExtensionIdSet id_difference = base::STLSetDifference<ExtensionIdSet>(
      registry()->enabled_extensions().GetIDs(), current_ids);
  ASSERT_EQ(1u, id_difference.size());
  // The new extension should have the same path.
  EXPECT_EQ(
      path,
      registry()->enabled_extensions().GetByID(*id_difference.begin())->path());

  path = data_dir().AppendASCII("empty_manifest");
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&path);

  // Try loading a bad extension (it should fail, and we should get an error).
  function = new api::DeveloperPrivateLoadUnpackedFunction();
  function->SetRenderFrameHost(web_contents->GetMainFrame());
  base::ListValue unpacked_args;
  std::unique_ptr<base::DictionaryValue> options(new base::DictionaryValue());
  options->SetBoolean("failQuietly", true);
  unpacked_args.Append(std::move(options));
  current_ids = registry()->enabled_extensions().GetIDs();
  EXPECT_FALSE(RunFunction(function, unpacked_args));
  EXPECT_EQ(manifest_errors::kManifestUnreadable, function->GetError());
  // We should have no new extensions installed.
  EXPECT_EQ(0u, base::STLSetDifference<ExtensionIdSet>(
                    registry()->enabled_extensions().GetIDs(),
                    current_ids).size());
}

TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateLoadUnpackedLoadError) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  {
    // Load an extension with a clear manifest error ('version' is invalid).
    TestExtensionDir dir;
    dir.WriteManifest(
        R"({
             "name": "foo",
             "description": "bar",
             "version": 1,
             "manifest_version": 2
           })");
    base::FilePath path = dir.UnpackedPath();
    api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&path);

    scoped_refptr<ExtensionFunction> function(
        new api::DeveloperPrivateLoadUnpackedFunction());
    function->SetRenderFrameHost(web_contents->GetMainFrame());
    std::unique_ptr<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(),
            "[{\"failQuietly\": true, \"populateError\": true}]", profile());
    // The loadError result should be populated.
    ASSERT_TRUE(result);
    std::unique_ptr<api::developer_private::LoadError> error =
        api::developer_private::LoadError::FromValue(*result);
    ASSERT_TRUE(error);
    ASSERT_TRUE(error->source);
    // The source should have *something* (rely on file highlighter tests for
    // the correct population).
    EXPECT_FALSE(error->source->before_highlight.empty());
    // The error should be appropriate (mentioning that version was invalid).
    EXPECT_TRUE(error->error.find("version") != std::string::npos)
        << error->error;
  }

  {
    // Load an extension with no manifest.
    TestExtensionDir dir;
    base::FilePath path = dir.UnpackedPath();
    api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&path);

    scoped_refptr<ExtensionFunction> function(
        new api::DeveloperPrivateLoadUnpackedFunction());
    function->SetRenderFrameHost(web_contents->GetMainFrame());
    std::unique_ptr<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(),
            "[{\"failQuietly\": true, \"populateError\": true}]", profile());
    // The load error should be populated.
    ASSERT_TRUE(result);
    std::unique_ptr<api::developer_private::LoadError> error =
        api::developer_private::LoadError::FromValue(*result);
    ASSERT_TRUE(error);
    // The file source should be empty.
    ASSERT_TRUE(error->source);
    EXPECT_TRUE(error->source->before_highlight.empty());
    EXPECT_TRUE(error->source->highlight.empty());
    EXPECT_TRUE(error->source->after_highlight.empty());
  }

  {
    // Load a valid extension.
    TestExtensionDir dir;
    dir.WriteManifest(
        R"({
             "name": "foo",
             "description": "bar",
             "version": "1.0",
             "manifest_version": 2
           })");
    base::FilePath path = dir.UnpackedPath();
    api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&path);

    scoped_refptr<ExtensionFunction> function(
        new api::DeveloperPrivateLoadUnpackedFunction());
    function->SetRenderFrameHost(web_contents->GetMainFrame());
    std::unique_ptr<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(),
            "[{\"failQuietly\": true, \"populateError\": true}]", profile());
    // There should be no load error.
    ASSERT_FALSE(result);
  }
}

// Test that the retryGuid supplied by loadUnpacked works correctly.
TEST_F(DeveloperPrivateApiUnitTest, LoadUnpackedRetryId) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  // Load an extension with a clear manifest error ('version' is invalid).
  TestExtensionDir dir;
  dir.WriteManifest(
      R"({
           "name": "foo",
           "description": "bar",
           "version": 1,
           "manifest_version": 2
         })");
  base::FilePath path = dir.UnpackedPath();
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&path);

  DeveloperPrivateAPI::UnpackedRetryId retry_guid;
  {
    // Trying to load the extension should result in a load error with the
    // retry id populated.
    scoped_refptr<ExtensionFunction> function(
        new api::DeveloperPrivateLoadUnpackedFunction());
    function->SetRenderFrameHost(web_contents->GetMainFrame());
    std::unique_ptr<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(),
            "[{\"failQuietly\": true, \"populateError\": true}]", profile());
    ASSERT_TRUE(result);
    std::unique_ptr<api::developer_private::LoadError> error =
        api::developer_private::LoadError::FromValue(*result);
    ASSERT_TRUE(error);
    EXPECT_FALSE(error->retry_guid.empty());
    retry_guid = error->retry_guid;
  }

  {
    // Trying to reload the same extension, again to fail, should result in the
    // same retry id.  This is somewhat an implementation detail, but is
    // important to ensure we don't allocate crazy numbers of ids if the user
    // just retries continuously.
    scoped_refptr<ExtensionFunction> function(
        new api::DeveloperPrivateLoadUnpackedFunction());
    function->SetRenderFrameHost(web_contents->GetMainFrame());
    std::unique_ptr<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(),
            "[{\"failQuietly\": true, \"populateError\": true}]", profile());
    ASSERT_TRUE(result);
    std::unique_ptr<api::developer_private::LoadError> error =
        api::developer_private::LoadError::FromValue(*result);
    ASSERT_TRUE(error);
    EXPECT_EQ(retry_guid, error->retry_guid);
  }

  {
    // Try loading a different directory. The retry id should be different; this
    // also tests loading a second extension with one retry currently
    // "in-flight" (i.e., unresolved).
    TestExtensionDir second_dir;
    second_dir.WriteManifest(
        R"({
             "name": "foo",
             "description": "bar",
             "version": 1,
             "manifest_version": 2
           })");
    base::FilePath second_path = second_dir.UnpackedPath();
    api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&second_path);

    scoped_refptr<ExtensionFunction> function(
        new api::DeveloperPrivateLoadUnpackedFunction());
    function->SetRenderFrameHost(web_contents->GetMainFrame());
    std::unique_ptr<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(),
            "[{\"failQuietly\": true, \"populateError\": true}]", profile());
    // The loadError result should be populated.
    ASSERT_TRUE(result);
    std::unique_ptr<api::developer_private::LoadError> error =
        api::developer_private::LoadError::FromValue(*result);
    ASSERT_TRUE(error);
    EXPECT_NE(retry_guid, error->retry_guid);
  }

  // Correct the manifest to make the extension valid.
  dir.WriteManifest(
      R"({
           "name": "foo",
           "description": "bar",
           "version": "1.0",
           "manifest_version": 2
         })");

  // Set the picker to choose an invalid path (the picker should be skipped if
  // we supply a retry id).
  base::FilePath empty_path;
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&empty_path);

  {
    // Try reloading the extension by supplying the retry id. It should succeed.
    scoped_refptr<ExtensionFunction> function(
        new api::DeveloperPrivateLoadUnpackedFunction());
    function->SetRenderFrameHost(web_contents->GetMainFrame());
    TestExtensionRegistryObserver observer(registry());
    api_test_utils::RunFunction(function.get(),
                                base::StringPrintf("[{\"failQuietly\": true,"
                                                   "\"populateError\": true,"
                                                   "\"retryGuid\": \"%s\"}]",
                                                   retry_guid.c_str()),
                                profile());
    const Extension* extension = observer.WaitForExtensionLoaded();
    ASSERT_TRUE(extension);
    EXPECT_EQ(extension->path(), path);
  }

  {
    // Try supplying an invalid retry id. It should fail with an error.
    scoped_refptr<ExtensionFunction> function(
        new api::DeveloperPrivateLoadUnpackedFunction());
    function->SetRenderFrameHost(web_contents->GetMainFrame());
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function.get(),
        "[{\"failQuietly\": true,"
        "\"populateError\": true,"
        "\"retryGuid\": \"invalid id\"}]",
        profile());
    EXPECT_EQ("Invalid retry id", error);
  }
}

// Tests calling "reload" on an unpacked extension with a manifest error,
// resulting in the reload failing. The reload call should then respond with
// the load error, which includes a retry GUID to be passed to loadUnpacked().
TEST_F(DeveloperPrivateApiUnitTest, ReloadBadExtensionToLoadUnpackedRetry) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  // A broken manifest (version's value should be a string).
  constexpr const char kBadManifest[] =
      R"({
           "name": "foo",
           "description": "bar",
           "version": 1,
           "manifest_version": 2
         })";
  constexpr const char kGoodManifest[] =
      R"({
           "name": "foo",
           "description": "bar",
           "version": "1",
           "manifest_version": 2
         })";

  // Create a good unpacked extension.
  TestExtensionDir dir;
  dir.WriteManifest(kGoodManifest);
  base::FilePath path = dir.UnpackedPath();
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&path);

  scoped_refptr<const Extension> extension;
  {
    ChromeTestExtensionLoader loader(profile());
    loader.set_pack_extension(false);
    extension = loader.LoadExtension(path);
  }
  ASSERT_TRUE(extension);
  const ExtensionId id = extension->id();

  std::string reload_args = base::StringPrintf(
      R"(["%s", {"failQuietly": true, "populateErrorForUnpacked":true}])",
      id.c_str());

  {
    // Try reloading while the manifest is still good. This should succeed, and
    // the extension should still be enabled. Additionally, the function should
    // wait for the reload to complete, so we should see an unload and reload.
    class UnloadedRegistryObserver : public ExtensionRegistryObserver {
     public:
      UnloadedRegistryObserver(const base::FilePath& expected_path,
                               ExtensionRegistry* registry)
          : expected_path_(expected_path), observer_(this) {
        observer_.Add(registry);
      }

      void OnExtensionUnloaded(content::BrowserContext* browser_context,
                               const Extension* extension,
                               UnloadedExtensionReason reason) override {
        ASSERT_FALSE(saw_unload_);
        saw_unload_ = extension->path() == expected_path_;
      }

      bool saw_unload() const { return saw_unload_; }

     private:
      bool saw_unload_ = false;
      base::FilePath expected_path_;
      ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver> observer_;

      DISALLOW_COPY_AND_ASSIGN(UnloadedRegistryObserver);
    };

    UnloadedRegistryObserver unload_observer(path, registry());
    auto function =
        base::MakeRefCounted<api::DeveloperPrivateReloadFunction>();
    function->SetRenderFrameHost(web_contents->GetMainFrame());
    api_test_utils::RunFunction(function.get(), reload_args, profile());
    // Note: no need to validate a saw_load()-type method because the presence
    // in enabled_extensions() indicates the extension was loaded.
    EXPECT_TRUE(unload_observer.saw_unload());
    EXPECT_TRUE(registry()->enabled_extensions().Contains(id));
  }

  dir.WriteManifest(kBadManifest);

  DeveloperPrivateAPI::UnpackedRetryId retry_guid;
  {
    // Trying to load the extension should result in a load error with the
    // retry GUID populated.
    auto function = base::MakeRefCounted<api::DeveloperPrivateReloadFunction>();
    function->SetRenderFrameHost(web_contents->GetMainFrame());
    std::unique_ptr<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), reload_args, profile());
    ASSERT_TRUE(result);
    std::unique_ptr<api::developer_private::LoadError> error =
        api::developer_private::LoadError::FromValue(*result);
    ASSERT_TRUE(error);
    EXPECT_FALSE(error->retry_guid.empty());
    retry_guid = error->retry_guid;
    EXPECT_TRUE(registry()->disabled_extensions().Contains(id));
  }

  dir.WriteManifest(kGoodManifest);
  {
    // Try reloading the extension by supplying the retry id. It should succeed,
    // and the extension should be enabled again.
    auto function =
        base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
    function->SetRenderFrameHost(web_contents->GetMainFrame());
    TestExtensionRegistryObserver observer(registry());
    std::string args =
        base::StringPrintf(R"([{"failQuietly": true, "populateError": true,
                                "retryGuid": "%s"}])",
                           retry_guid.c_str());
    api_test_utils::RunFunction(function.get(), args, profile());
    const Extension* extension = observer.WaitForExtensionLoaded();
    ASSERT_TRUE(extension);
    EXPECT_EQ(extension->path(), path);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(id));
  }
}

TEST_F(DeveloperPrivateApiUnitTest,
       DeveloperPrivateNotifyDragInstallInProgress) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  TestExtensionDir dir;
  dir.WriteManifest(
      R"({
           "name": "foo",
           "description": "bar",
           "version": "1",
           "manifest_version": 2
         })");
  base::FilePath path = dir.UnpackedPath();
  api::DeveloperPrivateNotifyDragInstallInProgressFunction::
      SetDropPathForTesting(&path);

  {
    auto function = base::MakeRefCounted<
        api::DeveloperPrivateNotifyDragInstallInProgressFunction>();
    function->SetRenderFrameHost(web_contents->GetMainFrame());
    api_test_utils::RunFunction(function.get(), "[]", profile());
  }

  // Set the picker to choose an invalid path (the picker should be skipped if
  // we supply a retry id).
  base::FilePath empty_path;
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&empty_path);

  constexpr char kLoadUnpackedArgs[] =
      R"([{"failQuietly": true,
           "populateError": true,
           "useDraggedPath": true}])";

  {
    // Try reloading the extension by supplying the retry id. It should succeed.
    auto function =
        base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
    function->SetRenderFrameHost(web_contents->GetMainFrame());
    TestExtensionRegistryObserver observer(registry());
    api_test_utils::RunFunction(function.get(), kLoadUnpackedArgs, profile());
    const Extension* extension = observer.WaitForExtensionLoaded();
    ASSERT_TRUE(extension);
    EXPECT_EQ(extension->path(), path);
  }

  // Next, ensure that nothing catastrophic happens if the file that was dropped
  // was not a directory. In theory, this shouldn't happen (the JS validates the
  // file), but it could in the case of a compromised renderer, JS bug, etc.
  base::FilePath invalid_path = path.AppendASCII("manifest.json");
  api::DeveloperPrivateNotifyDragInstallInProgressFunction::
      SetDropPathForTesting(&invalid_path);
  {
    auto function = base::MakeRefCounted<
        api::DeveloperPrivateNotifyDragInstallInProgressFunction>();
    function->SetRenderFrameHost(web_contents->GetMainFrame());
    std::unique_ptr<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(function.get(), "[]",
                                                         profile());
  }

  {
    // Trying to load the bad extension (the path points to the manifest, not
    // the directory) should result in a load error.
    auto function =
        base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
    function->SetRenderFrameHost(web_contents->GetMainFrame());
    TestExtensionRegistryObserver observer(registry());
    std::unique_ptr<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), kLoadUnpackedArgs, profile());
    ASSERT_TRUE(result);
    EXPECT_TRUE(api::developer_private::LoadError::FromValue(*result));
  }

  // Cleanup.
  api::DeveloperPrivateNotifyDragInstallInProgressFunction::
      SetDropPathForTesting(nullptr);
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(nullptr);
}

// Test developerPrivate.requestFileSource.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateRequestFileSource) {
  // Testing of this function seems light, but that's because it basically just
  // forwards to reading a file to a string, and highlighting it - both of which
  // are already tested separately.
  const Extension* extension = LoadUnpackedExtension();
  const char kErrorMessage[] = "Something went wrong";
  api::developer_private::RequestFileSourceProperties properties;
  properties.extension_id = extension->id();
  properties.path_suffix = "manifest.json";
  properties.message = kErrorMessage;
  properties.manifest_key.reset(new std::string("name"));

  scoped_refptr<ExtensionFunction> function(
      new api::DeveloperPrivateRequestFileSourceFunction());
  base::ListValue file_source_args;
  file_source_args.Append(properties.ToValue());
  EXPECT_TRUE(RunFunction(function, file_source_args)) << function->GetError();

  const base::Value* response_value = nullptr;
  ASSERT_TRUE(function->GetResultList()->Get(0u, &response_value));
  std::unique_ptr<api::developer_private::RequestFileSourceResponse> response =
      api::developer_private::RequestFileSourceResponse::FromValue(
          *response_value);
  EXPECT_FALSE(response->before_highlight.empty());
  EXPECT_EQ("\"name\": \"foo\"", response->highlight);
  EXPECT_FALSE(response->after_highlight.empty());
  EXPECT_EQ("foo: manifest.json", response->title);
  EXPECT_EQ(kErrorMessage, response->message);
}

// Test developerPrivate.getExtensionsInfo.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateGetExtensionsInfo) {
  LoadSimpleExtension();

  // The test here isn't so much about the generated value (that's tested in
  // ExtensionInfoGenerator's unittest), but rather just to make sure we can
  // serialize/deserialize the result - which implicity tests that everything
  // has a sane value.
  scoped_refptr<ExtensionFunction> function(
      new api::DeveloperPrivateGetExtensionsInfoFunction());
  EXPECT_TRUE(RunFunction(function, base::ListValue())) << function->GetError();
  const base::ListValue* results = function->GetResultList();
  ASSERT_EQ(1u, results->GetSize());
  const base::ListValue* list = nullptr;
  ASSERT_TRUE(results->GetList(0u, &list));
  ASSERT_EQ(1u, list->GetSize());
  const base::Value* value = nullptr;
  ASSERT_TRUE(list->Get(0u, &value));
  std::unique_ptr<api::developer_private::ExtensionInfo> info =
      api::developer_private::ExtensionInfo::FromValue(*value);
  ASSERT_TRUE(info);

  // As a sanity check, also run the GetItemsInfo and make sure it returns a
  // sane value.
  function = new api::DeveloperPrivateGetItemsInfoFunction();
  base::ListValue args;
  args.AppendBoolean(false);
  args.AppendBoolean(false);
  EXPECT_TRUE(RunFunction(function, args)) << function->GetError();
  results = function->GetResultList();
  ASSERT_EQ(1u, results->GetSize());
  ASSERT_TRUE(results->GetList(0u, &list));
  ASSERT_EQ(1u, list->GetSize());
  ASSERT_TRUE(list->Get(0u, &value));
  std::unique_ptr<api::developer_private::ItemInfo> item_info =
      api::developer_private::ItemInfo::FromValue(*value);
  ASSERT_TRUE(item_info);
}

// Test developerPrivate.deleteExtensionErrors.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateDeleteExtensionErrors) {
  FeatureSwitch::ScopedOverride error_console_override(
      FeatureSwitch::error_console(), true);
  profile()->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);
  const Extension* extension = LoadSimpleExtension();

  // Report some errors.
  ErrorConsole* error_console = ErrorConsole::Get(profile());
  error_console->SetReportingAllForExtension(extension->id(), true);
  error_console->ReportError(
      error_test_util::CreateNewRuntimeError(extension->id(), "foo"));
  error_console->ReportError(
      error_test_util::CreateNewRuntimeError(extension->id(), "bar"));
  error_console->ReportError(
      error_test_util::CreateNewManifestError(extension->id(), "baz"));
  EXPECT_EQ(3u, error_console->GetErrorsForExtension(extension->id()).size());

  // Start by removing all errors for the extension of a given type (manifest).
  std::string type_string = api::developer_private::ToString(
      api::developer_private::ERROR_TYPE_MANIFEST);
  std::unique_ptr<base::ListValue> args =
      ListBuilder()
          .Append(DictionaryBuilder()
                      .Set("extensionId", extension->id())
                      .Set("type", type_string)
                      .Build())
          .Build();
  scoped_refptr<ExtensionFunction> function =
      new api::DeveloperPrivateDeleteExtensionErrorsFunction();
  EXPECT_TRUE(RunFunction(function, *args)) << function->GetError();
  // Two errors should remain.
  const ErrorList& error_list =
      error_console->GetErrorsForExtension(extension->id());
  ASSERT_EQ(2u, error_list.size());

  // Next remove errors by id.
  int error_id = error_list[0]->id();
  args =
      ListBuilder()
          .Append(DictionaryBuilder()
                      .Set("extensionId", extension->id())
                      .Set("errorIds", ListBuilder().Append(error_id).Build())
                      .Build())
          .Build();
  function = new api::DeveloperPrivateDeleteExtensionErrorsFunction();
  EXPECT_TRUE(RunFunction(function, *args)) << function->GetError();
  // And then there was one.
  EXPECT_EQ(1u, error_console->GetErrorsForExtension(extension->id()).size());

  // Finally remove all errors for the extension.
  args =
      ListBuilder()
          .Append(
              DictionaryBuilder().Set("extensionId", extension->id()).Build())
          .Build();
  function = new api::DeveloperPrivateDeleteExtensionErrorsFunction();
  EXPECT_TRUE(RunFunction(function, *args)) << function->GetError();
  // No more errors!
  EXPECT_TRUE(error_console->GetErrorsForExtension(extension->id()).empty());
}

// Tests that developerPrivate.repair does not succeed for a non-corrupted
// extension.
TEST_F(DeveloperPrivateApiUnitTest, RepairNotBrokenExtension) {
  base::FilePath extension_path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(extension_path, INSTALL_NEW);

  // Attempt to repair the good extension, expect failure.
  std::unique_ptr<base::ListValue> args =
      ListBuilder().Append(extension->id()).Build();
  scoped_refptr<ExtensionFunction> function =
      new api::DeveloperPrivateRepairExtensionFunction();
  EXPECT_FALSE(RunFunction(function, *args));
  EXPECT_EQ("Cannot repair a healthy extension.", function->GetError());
}

// Tests that developerPrivate.private cannot repair a policy-installed
// extension.
// Regression test for https://crbug.com/577959.
TEST_F(DeveloperPrivateApiUnitTest, RepairPolicyExtension) {
  std::string extension_id(kGoodCrx);

  // Set up a mock provider with a policy extension.
  std::unique_ptr<MockExternalProvider> mock_provider =
      std::make_unique<MockExternalProvider>(
          service(), Manifest::EXTERNAL_POLICY_DOWNLOAD);
  MockExternalProvider* mock_provider_ptr = mock_provider.get();
  AddMockExternalProvider(std::move(mock_provider));
  mock_provider_ptr->UpdateOrAddExtension(extension_id, "1.0.0.0",
                                          data_dir().AppendASCII("good.crx"));
  // Reloading extensions should find our externally registered extension
  // and install it.
  content::WindowedNotificationObserver observer(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources());
  service()->CheckForExternalUpdates();
  observer.Wait();

  // Attempt to repair the good extension, expect failure.
  std::unique_ptr<base::ListValue> args =
      ListBuilder().Append(extension_id).Build();
  scoped_refptr<ExtensionFunction> function =
      new api::DeveloperPrivateRepairExtensionFunction();
  EXPECT_FALSE(RunFunction(function, *args));
  EXPECT_EQ("Cannot repair a healthy extension.", function->GetError());

  // Corrupt the extension , still expect repair failure because this is a
  // policy extension.
  service()->DisableExtension(extension_id, disable_reason::DISABLE_CORRUPTED);
  args = ListBuilder().Append(extension_id).Build();
  function = new api::DeveloperPrivateRepairExtensionFunction();
  EXPECT_FALSE(RunFunction(function, *args));
  EXPECT_EQ("Cannot repair a policy-installed extension.",
            function->GetError());
}

// Test developerPrivate.updateProfileConfiguration: Try to turn on devMode
// when DeveloperToolsAvailability policy disallows developer tools.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateDevModeDisabledPolicy) {
  testing_pref_service()->SetManagedPref(prefs::kExtensionsUIDeveloperMode,
                                         std::make_unique<base::Value>(false));

  UpdateProfileConfigurationDevMode(true);

  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode));

  std::unique_ptr<api::developer_private::ProfileInfo> profile_info;
  ASSERT_NO_FATAL_FAILURE(GetProfileConfiguration(&profile_info));
  EXPECT_FALSE(profile_info->in_developer_mode);
  EXPECT_TRUE(profile_info->is_developer_mode_controlled_by_policy);
}

// Test developerPrivate.updateProfileConfiguration: Try to turn on devMode
// (without DeveloperToolsAvailability policy).
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateDevMode) {
  UpdateProfileConfigurationDevMode(false);
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode));
  {
    std::unique_ptr<api::developer_private::ProfileInfo> profile_info;
    ASSERT_NO_FATAL_FAILURE(GetProfileConfiguration(&profile_info));
    EXPECT_FALSE(profile_info->in_developer_mode);
    EXPECT_FALSE(profile_info->is_developer_mode_controlled_by_policy);
  }

  UpdateProfileConfigurationDevMode(true);
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode));
  {
    std::unique_ptr<api::developer_private::ProfileInfo> profile_info;
    ASSERT_NO_FATAL_FAILURE(GetProfileConfiguration(&profile_info));
    EXPECT_TRUE(profile_info->in_developer_mode);
    EXPECT_FALSE(profile_info->is_developer_mode_controlled_by_policy);
  }
}

TEST_F(DeveloperPrivateApiUnitTest, LoadUnpackedFailsWithoutDevMode) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  base::FilePath path = data_dir().AppendASCII("simple_with_popup");
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&path);

  PrefService* prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kExtensionsUIDeveloperMode, false);
  scoped_refptr<ExtensionFunction> function =
      base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
  function->SetRenderFrameHost(web_contents->GetMainFrame());
  std::string error = extension_function_test_utils::RunFunctionAndReturnError(
      function.get(), "[]", browser());
  EXPECT_THAT(error, testing::HasSubstr("developer mode"));
  prefs->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);
}

TEST_F(DeveloperPrivateApiUnitTest, LoadUnpackedFailsWithBlacklistingPolicy) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  base::FilePath path = data_dir().AppendASCII("simple_with_popup");
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&path);

  {
    ExtensionManagementPrefUpdater<sync_preferences::TestingPrefServiceSyncable>
        pref_updater(testing_profile()->GetTestingPrefService());
    pref_updater.SetBlacklistedByDefault(true);
  }
  EXPECT_TRUE(
      ExtensionManagementFactory::GetForBrowserContext(browser_context())
          ->BlacklistedByDefault());

  EXPECT_FALSE(
      ExtensionManagementFactory::GetForBrowserContext(browser_context())
          ->HasWhitelistedExtension());

  auto info = DeveloperPrivateAPI::CreateProfileInfo(testing_profile());

  EXPECT_FALSE(info->can_load_unpacked);

  scoped_refptr<ExtensionFunction> function =
      base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
  function->SetRenderFrameHost(web_contents->GetMainFrame());
  std::string error = extension_function_test_utils::RunFunctionAndReturnError(
      function.get(), "[]", browser());
  EXPECT_THAT(error, testing::HasSubstr("policy"));
}

TEST_F(DeveloperPrivateApiUnitTest,
       LoadUnpackedWorksWithBlacklistingPolicyAlongWhitelistingPolicy) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  base::FilePath path = data_dir().AppendASCII("simple_with_popup");
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&path);

  {
    ExtensionManagementPrefUpdater<sync_preferences::TestingPrefServiceSyncable>
        pref_updater(testing_profile()->GetTestingPrefService());
    pref_updater.SetBlacklistedByDefault(true);
    pref_updater.SetIndividualExtensionInstallationAllowed(kGoodCrx, true);
  }

  EXPECT_TRUE(
      ExtensionManagementFactory::GetForBrowserContext(browser_context())
          ->BlacklistedByDefault());

  EXPECT_TRUE(
      ExtensionManagementFactory::GetForBrowserContext(browser_context())
          ->HasWhitelistedExtension());

  auto info = DeveloperPrivateAPI::CreateProfileInfo(testing_profile());

  EXPECT_TRUE(info->can_load_unpacked);
}

TEST_F(DeveloperPrivateApiUnitTest, InstallDroppedFileNoDraggedPath) {
  extensions::ExtensionInstallUI::set_disable_ui_for_tests();
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  scoped_refptr<ExtensionFunction> function =
      base::MakeRefCounted<api::DeveloperPrivateInstallDroppedFileFunction>();
  function->SetRenderFrameHost(web_contents->GetMainFrame());

  TestExtensionRegistryObserver observer(registry());
  EXPECT_EQ("No dragged path", api_test_utils::RunFunctionAndReturnError(
                                   function.get(), "[]", profile()));
}

TEST_F(DeveloperPrivateApiUnitTest, InstallDroppedFileCrx) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "foo",
           "version": "1.0",
           "manifest_version": 2
         })");
  base::FilePath crx_path = test_dir.Pack();
  extensions::ExtensionInstallUI::set_disable_ui_for_tests();
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  DeveloperPrivateAPI::Get(profile())->SetDraggedPath(web_contents.get(),
                                                      crx_path);

  scoped_refptr<ExtensionFunction> function =
      base::MakeRefCounted<api::DeveloperPrivateInstallDroppedFileFunction>();
  function->SetRenderFrameHost(web_contents->GetMainFrame());

  TestExtensionRegistryObserver observer(registry());
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), "[]", profile()))
      << function->GetError();
  const Extension* extension = observer.WaitForExtensionInstalled();
  ASSERT_TRUE(extension);
  EXPECT_EQ("foo", extension->name());
}

TEST_F(DeveloperPrivateApiUnitTest, InstallDroppedFileUserScript) {
  base::FilePath script_path =
      data_dir().AppendASCII("user_script_basic.user.js");
  extensions::ExtensionInstallUI::set_disable_ui_for_tests();
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  DeveloperPrivateAPI::Get(profile())->SetDraggedPath(web_contents.get(),
                                                      script_path);

  scoped_refptr<ExtensionFunction> function =
      base::MakeRefCounted<api::DeveloperPrivateInstallDroppedFileFunction>();
  function->SetRenderFrameHost(web_contents->GetMainFrame());

  TestExtensionRegistryObserver observer(registry());
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), "[]", profile()))
      << function->GetError();
  const Extension* extension = observer.WaitForExtensionInstalled();
  ASSERT_TRUE(extension);
  EXPECT_EQ("My user script", extension->name());
}

TEST_F(DeveloperPrivateApiUnitTest, GrantHostPermission) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test").AddPermission("<all_urls>").Build();
  service()->AddExtension(extension.get());
  ScriptingPermissionsModifier modifier(profile(), extension.get());
  EXPECT_FALSE(modifier.HasWithheldHostPermissions());
  modifier.SetWithholdHostPermissions(true);

  auto run_add_host_permission = [this, extension](base::StringPiece host,
                                                   bool should_succeed,
                                                   const char* expected_error) {
    SCOPED_TRACE(host);
    scoped_refptr<ExtensionFunction> function =
        base::MakeRefCounted<api::DeveloperPrivateAddHostPermissionFunction>();

    std::string args = base::StringPrintf(R"(["%s", "%s"])",
                                          extension->id().c_str(), host.data());
    if (should_succeed) {
      EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile()))
          << function->GetError();
    } else {
      EXPECT_EQ(expected_error, api_test_utils::RunFunctionAndReturnError(
                                    function.get(), args, profile()));
    }
  };

  const GURL kExampleCom("https://example.com/");
  EXPECT_FALSE(modifier.HasGrantedHostPermission(kExampleCom));
  run_add_host_permission("https://example.com/*", true, nullptr);
  EXPECT_TRUE(modifier.HasGrantedHostPermission(kExampleCom));

  const GURL kGoogleCom("https://google.com");
  const GURL kMapsGoogleCom("https://maps.google.com/");
  EXPECT_FALSE(modifier.HasGrantedHostPermission(kGoogleCom));
  EXPECT_FALSE(modifier.HasGrantedHostPermission(kMapsGoogleCom));
  run_add_host_permission("https://*.google.com/*", true, nullptr);
  EXPECT_TRUE(modifier.HasGrantedHostPermission(kGoogleCom));
  EXPECT_TRUE(modifier.HasGrantedHostPermission(kMapsGoogleCom));

  run_add_host_permission(kInvalidHost, false, kInvalidHostError);
  // Path of the pattern must exactly match "/*".
  run_add_host_permission("https://example.com/", false, kInvalidHostError);
  run_add_host_permission("https://example.com/foobar", false,
                          kInvalidHostError);
  run_add_host_permission("https://example.com/#foobar", false,
                          kInvalidHostError);
  run_add_host_permission("https://example.com/*foobar", false,
                          kInvalidHostError);

  // Cannot grant chrome:-scheme URLs.
  GURL chrome_host("chrome://settings/*");
  run_add_host_permission(chrome_host.spec(), false, kInvalidHostError);

  EXPECT_FALSE(modifier.HasGrantedHostPermission(chrome_host));
}

TEST_F(DeveloperPrivateApiUnitTest, RemoveHostPermission) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test").AddPermission("<all_urls>").Build();
  service()->AddExtension(extension.get());
  ScriptingPermissionsModifier modifier(profile(), extension.get());
  EXPECT_FALSE(modifier.HasWithheldHostPermissions());
  modifier.SetWithholdHostPermissions(true);

  auto run_remove_host_permission = [this, extension](
                                        base::StringPiece host,
                                        bool should_succeed,
                                        const char* expected_error) {
    SCOPED_TRACE(host);
    scoped_refptr<ExtensionFunction> function = base::MakeRefCounted<
        api::DeveloperPrivateRemoveHostPermissionFunction>();
    std::string args = base::StringPrintf(R"(["%s", "%s"])",
                                          extension->id().c_str(), host.data());
    if (should_succeed) {
      EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile()))
          << function->GetError();
    } else {
      EXPECT_EQ(expected_error, api_test_utils::RunFunctionAndReturnError(
                                    function.get(), args, profile()));
    }
  };

  run_remove_host_permission("https://example.com/*", false,
                             "Cannot remove a host that hasn't been granted.");

  const GURL kExampleCom("https://example.com");
  modifier.GrantHostPermission(kExampleCom);
  EXPECT_TRUE(modifier.HasGrantedHostPermission(kExampleCom));

  // Path of the pattern must exactly match "/*".
  run_remove_host_permission("https://example.com/", false, kInvalidHostError);
  run_remove_host_permission("https://example.com/foobar", false,
                             kInvalidHostError);
  run_remove_host_permission("https://example.com/#foobar", false,
                             kInvalidHostError);
  run_remove_host_permission("https://example.com/*foobar", false,
                             kInvalidHostError);
  run_remove_host_permission(kInvalidHost, false, kInvalidHostError);
  EXPECT_TRUE(modifier.HasGrantedHostPermission(kExampleCom));

  run_remove_host_permission("https://example.com/*", true, nullptr);
  EXPECT_FALSE(modifier.HasGrantedHostPermission(kExampleCom));

  URLPattern new_pattern(Extension::kValidHostPermissionSchemes,
                         "https://*.google.com/*");
  permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
      profile(), *extension,
      PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                    URLPatternSet({new_pattern}), URLPatternSet()));

  const GURL kGoogleCom("https://google.com/");
  const GURL kMapsGoogleCom("https://maps.google.com/");
  EXPECT_TRUE(modifier.HasGrantedHostPermission(kGoogleCom));
  EXPECT_TRUE(modifier.HasGrantedHostPermission(kMapsGoogleCom));

  run_remove_host_permission("https://*.google.com/*", true, nullptr);
  EXPECT_FALSE(modifier.HasGrantedHostPermission(kGoogleCom));
  EXPECT_FALSE(modifier.HasGrantedHostPermission(kMapsGoogleCom));
}

// This test is flaky on chromeos.
// https://crbug.com/937355
#if defined(OS_CHROMEOS)
#define MAYBE_UpdateHostAccess DISABLED_UpdateHostAccess
#else
#define MAYBE_UpdateHostAccess UpdateHostAccess
#endif
TEST_F(DeveloperPrivateApiUnitTest, MAYBE_UpdateHostAccess) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test").AddPermission("<all_urls>").Build();
  service()->AddExtension(extension.get());
  ScriptingPermissionsModifier modifier(profile(), extension.get());

  EXPECT_FALSE(modifier.HasWithheldHostPermissions());

  RunUpdateHostAccess(*extension, "ON_CLICK");
  EXPECT_TRUE(modifier.HasWithheldHostPermissions());

  RunUpdateHostAccess(*extension, "ON_ALL_SITES");
  EXPECT_FALSE(modifier.HasWithheldHostPermissions());

  RunUpdateHostAccess(*extension, "ON_SPECIFIC_SITES");
  EXPECT_TRUE(modifier.HasWithheldHostPermissions());
}

TEST_F(DeveloperPrivateApiUnitTest,
       UpdateHostAccess_SpecificSitesRemovedOnTransitionToOnClick) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test").AddPermission("<all_urls>").Build();
  service()->AddExtension(extension.get());
  ScriptingPermissionsModifier modifier(profile(), extension.get());
  modifier.SetWithholdHostPermissions(true);

  RunUpdateHostAccess(*extension, "ON_SPECIFIC_SITES");
  const GURL example_com("https://example.com");
  modifier.GrantHostPermission(example_com);
  EXPECT_TRUE(modifier.HasWithheldHostPermissions());
  EXPECT_TRUE(modifier.HasGrantedHostPermission(example_com));

  RunUpdateHostAccess(*extension, "ON_CLICK");
  EXPECT_TRUE(modifier.HasWithheldHostPermissions());
  EXPECT_FALSE(modifier.HasGrantedHostPermission(example_com));

  // NOTE(devlin): It's a bit unfortunate that by cycling between host access
  // settings, a user loses any stored state. This would be painful if the user
  // had set "always run on foo" for a dozen or so sites, and accidentally
  // changed the setting.
  // There are ways we could address this, such as introducing a tri-state for
  // the preference and keeping a stored set of any granted host permissions,
  // but this then results in a funny edge case:
  // - User has "on specific sites" set, with access to example.com and
  //   chromium.org granted.
  // - User changes to "on click" -> no sites are granted.
  // - User visits google.com, and says "always run on this site." This changes
  //   the setting back to "on specific sites", and will implicitly re-grant
  //   example.com and chromium.org permissions, without any additional
  //   prompting.
  // To avoid this, we just clear any granted permissions when the user
  // transitions between states. Since this is definitely a power-user surface,
  // this is likely okay.
  RunUpdateHostAccess(*extension, "ON_SPECIFIC_SITES");
  EXPECT_TRUE(modifier.HasWithheldHostPermissions());
  EXPECT_FALSE(modifier.HasGrantedHostPermission(example_com));
}

TEST_F(DeveloperPrivateApiUnitTest,
       UpdateHostAccess_SpecificSitesRemovedOnTransitionToAllSites) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test").AddPermission("<all_urls>").Build();
  service()->AddExtension(extension.get());
  ScriptingPermissionsModifier modifier(profile(), extension.get());
  modifier.SetWithholdHostPermissions(true);

  RunUpdateHostAccess(*extension, "ON_SPECIFIC_SITES");
  const GURL example_com("https://example.com");
  modifier.GrantHostPermission(example_com);
  EXPECT_TRUE(modifier.HasWithheldHostPermissions());
  EXPECT_TRUE(modifier.HasGrantedHostPermission(example_com));

  RunUpdateHostAccess(*extension, "ON_ALL_SITES");
  EXPECT_FALSE(modifier.HasWithheldHostPermissions());
  EXPECT_TRUE(modifier.HasGrantedHostPermission(example_com));

  RunUpdateHostAccess(*extension, "ON_SPECIFIC_SITES");
  EXPECT_TRUE(modifier.HasWithheldHostPermissions());
  EXPECT_FALSE(modifier.HasGrantedHostPermission(example_com));
}

TEST_F(DeveloperPrivateApiUnitTest,
       UpdateHostAccess_GrantScopeGreaterThanRequestedScope) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test").AddPermission("http://*/*").Build();
  service()->AddExtension(extension.get());
  ScriptingPermissionsModifier modifier(profile(), extension.get());
  modifier.SetWithholdHostPermissions(true);

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());
  EXPECT_EQ(PermissionSet(),
            extension->permissions_data()->active_permissions());
  EXPECT_EQ(PermissionSet(),
            *extension_prefs->GetRuntimeGrantedPermissions(extension->id()));

  {
    scoped_refptr<ExtensionFunction> function =
        base::MakeRefCounted<api::DeveloperPrivateAddHostPermissionFunction>();
    std::string args = base::StringPrintf(
        R"(["%s", "%s"])", extension->id().c_str(), "*://chromium.org/*");
    EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile()))
        << function->GetError();
  }

  // The active permissions (which are given to the extension process) should
  // only include the intersection of what was requested by the extension and
  // the runtime granted permissions - which is http://chromium.org/*.
  URLPattern http_chromium(Extension::kValidHostPermissionSchemes,
                           "http://chromium.org/*");
  const PermissionSet http_chromium_set(
      APIPermissionSet(), ManifestPermissionSet(),
      URLPatternSet({http_chromium}), URLPatternSet());
  EXPECT_EQ(http_chromium_set,
            extension->permissions_data()->active_permissions());

  // The runtime granted permissions should include all of what was approved by
  // the user, which is *://chromium.org/*, and should be present in both the
  // scriptable and explicit hosts.
  URLPattern all_chromium(Extension::kValidHostPermissionSchemes,
                          "*://chromium.org/*");
  const PermissionSet all_chromium_set(
      APIPermissionSet(), ManifestPermissionSet(),
      URLPatternSet({all_chromium}), URLPatternSet({all_chromium}));
  EXPECT_EQ(all_chromium_set,
            *extension_prefs->GetRuntimeGrantedPermissions(extension->id()));

  {
    scoped_refptr<ExtensionFunction> function = base::MakeRefCounted<
        api::DeveloperPrivateRemoveHostPermissionFunction>();
    std::string args = base::StringPrintf(
        R"(["%s", "%s"])", extension->id().c_str(), "*://chromium.org/*");
    EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile()))
        << function->GetError();
  }

  // Removing the granted permission should remove it entirely from both
  // the active and the stored permissions.
  EXPECT_EQ(PermissionSet(),
            extension->permissions_data()->active_permissions());
  EXPECT_EQ(PermissionSet(),
            *extension_prefs->GetRuntimeGrantedPermissions(extension->id()));
}

TEST_F(DeveloperPrivateApiUnitTest,
       UpdateHostAccess_UnrequestedHostsDispatchUpdateEvents) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test").AddPermission("http://google.com/*").Build();
  service()->AddExtension(extension.get());
  ScriptingPermissionsModifier modifier(profile(), extension.get());
  modifier.SetWithholdHostPermissions(true);

  // We need to call DeveloperPrivateAPI::Get() in order to instantiate the
  // keyed service, since it's not created by default in unit tests.
  DeveloperPrivateAPI::Get(profile());
  const ExtensionId listener_id = crx_file::id_util::GenerateId("listener");
  EventRouter* event_router = EventRouter::Get(profile());

  // The DeveloperPrivateEventRouter will only dispatch events if there's at
  // least one listener to dispatch to. Create one.
  content::RenderProcessHost* process = nullptr;
  const char* kEventName =
      api::developer_private::OnItemStateChanged::kEventName;
  event_router->AddEventListener(kEventName, process, listener_id);

  TestEventRouterObserver test_observer(event_router);
  EXPECT_FALSE(
      WasPermissionsUpdatedEventDispatched(test_observer, extension->id()));

  URLPatternSet hosts({URLPattern(Extension::kValidHostPermissionSchemes,
                                  "https://example.com/*")});
  PermissionSet permissions(APIPermissionSet(), ManifestPermissionSet(),
                            hosts.Clone(), hosts.Clone());
  permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
      profile(), *extension, permissions);

  // The event router fetches icons from a blocking thread when sending the
  // update event; allow it to finish before verifying the event was dispatched.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      WasPermissionsUpdatedEventDispatched(test_observer, extension->id()));

  test_observer.ClearEvents();

  permissions_test_util::RevokeRuntimePermissionsAndWaitForCompletion(
      profile(), *extension, permissions);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      WasPermissionsUpdatedEventDispatched(test_observer, extension->id()));
}

TEST_F(DeveloperPrivateApiUnitTest, ExtensionUpdatedEventOnPermissionsChange) {
  // We need to call DeveloperPrivateAPI::Get() in order to instantiate the
  // keyed service, since it's not created by default in unit tests.
  DeveloperPrivateAPI::Get(profile());
  const ExtensionId listener_id = crx_file::id_util::GenerateId("listener");
  EventRouter* event_router = EventRouter::Get(profile());

  // The DeveloperPrivateEventRouter will only dispatch events if there's at
  // least one listener to dispatch to. Create one.
  content::RenderProcessHost* process = nullptr;
  const char* kEventName =
      api::developer_private::OnItemStateChanged::kEventName;
  event_router->AddEventListener(kEventName, process, listener_id);

  scoped_refptr<const Extension> dummy_extension =
      ExtensionBuilder("dummy")
          .SetManifestKey("optional_permissions",
                          ListBuilder().Append("tabs").Build())
          .Build();

  TestEventRouterObserver test_observer(event_router);
  EXPECT_FALSE(WasPermissionsUpdatedEventDispatched(test_observer,
                                                    dummy_extension->id()));

  APIPermissionSet apis;
  apis.insert(APIPermission::kTab);
  PermissionSet permissions(std::move(apis), ManifestPermissionSet(),
                            URLPatternSet(), URLPatternSet());
  permissions_test_util::GrantOptionalPermissionsAndWaitForCompletion(
      profile(), *dummy_extension, permissions);

  // The event router fetches icons from a blocking thread when sending the
  // update event; allow it to finish before verifying the event was dispatched.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(WasPermissionsUpdatedEventDispatched(test_observer,
                                                   dummy_extension->id()));

  test_observer.ClearEvents();

  permissions_test_util::RevokeOptionalPermissionsAndWaitForCompletion(
      profile(), *dummy_extension, permissions,
      PermissionsUpdater::REMOVE_HARD);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(WasPermissionsUpdatedEventDispatched(test_observer,
                                                   dummy_extension->id()));
}

TEST_F(DeveloperPrivateApiUnitTest, InstallDroppedFileZip) {
  base::FilePath zip_path = data_dir().AppendASCII("simple_empty.zip");
  extensions::ExtensionInstallUI::set_disable_ui_for_tests();
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  DeveloperPrivateAPI::Get(profile())->SetDraggedPath(web_contents.get(),
                                                      zip_path);

  scoped_refptr<ExtensionFunction> function =
      base::MakeRefCounted<api::DeveloperPrivateInstallDroppedFileFunction>();
  function->SetRenderFrameHost(web_contents->GetMainFrame());

  TestExtensionRegistryObserver observer(registry());
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), "[]", profile()))
      << function->GetError();
  const Extension* extension = observer.WaitForExtensionInstalled();
  ASSERT_TRUE(extension);
  EXPECT_EQ("Simple Empty Extension", extension->name());
}

class DeveloperPrivateApiSupervisedUserUnitTest
    : public DeveloperPrivateApiUnitTest {
 public:
  DeveloperPrivateApiSupervisedUserUnitTest() = default;
  ~DeveloperPrivateApiSupervisedUserUnitTest() override = default;

  bool ProfileIsSupervised() const override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeveloperPrivateApiSupervisedUserUnitTest);
};

// Tests trying to call loadUnpacked when the profile shouldn't be allowed to.
TEST_F(DeveloperPrivateApiSupervisedUserUnitTest,
       LoadUnpackedFailsForSupervisedUsers) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  base::FilePath path = data_dir().AppendASCII("simple_with_popup");
  api::EntryPicker::SkipPickerAndAlwaysSelectPathForTest(&path);

  ASSERT_TRUE(profile()->IsSupervised());

  scoped_refptr<ExtensionFunction> function =
      base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
  function->SetRenderFrameHost(web_contents->GetMainFrame());
  std::string error = extension_function_test_utils::RunFunctionAndReturnError(
      function.get(), "[]", browser());
  EXPECT_THAT(error, testing::HasSubstr("Supervised"));
}

}  // namespace extensions
