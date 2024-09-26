// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/developer_private/extension_info_generator.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/permissions/permissions_test_util.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_install_ui.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/supervised_user/core/common/features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_error_test_util.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/mock_external_provider.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/test_event_router_observer.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/test_extension_dir.h"
#include "services/data_decoder/data_decoder_service.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace extensions {

namespace {

const char kGoodCrx[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char kGoogleOnlyCrx[] = "jjlcocfpfbknlbgijblaapbcpbdglkhf";
constexpr char kInvalidHost[] = "invalid host";
constexpr char kInvalidHostError[] = "Invalid host.";

std::unique_ptr<KeyedService> BuildAPI(content::BrowserContext* context) {
  return std::make_unique<DeveloperPrivateAPI>(context);
}

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* profile) {
  return std::make_unique<EventRouter>(profile, ExtensionPrefs::Get(profile));
}

bool HasPrefsPermission(bool (*has_pref)(const ExtensionId&,
                                         content::BrowserContext*),
                        content::BrowserContext* context,
                        const ExtensionId& id) {
  return has_pref(id, context);
}

bool WasItemChangedEventDispatched(
    const TestEventRouterObserver& observer,
    const ExtensionId& extension_id,
    const api::developer_private::EventType event_type) {
  const std::string kEventName =
      api::developer_private::OnItemStateChanged::kEventName;
  const auto& event_map = observer.events();
  auto iter = event_map.find(kEventName);
  if (iter == event_map.end())
    return false;

  const Event& event = *iter->second;
  CHECK_GE(1u, event.event_args.size());
  std::optional<api::developer_private::EventData> event_data =
      api::developer_private::EventData::FromValue(event.event_args[0]);
  if (!event_data)
    return false;

  if (event_data->item_id != extension_id ||
      event_data->event_type != event_type) {
    return false;
  }

  return true;
}

bool WasUserSiteSettingsChangedEventDispatched(
    const TestEventRouterObserver& observer,
    api::developer_private::UserSiteSettings* settings) {
  const std::string kEventName =
      api::developer_private::OnUserSiteSettingsChanged::kEventName;
  const auto& event_map = observer.events();
  auto iter = event_map.find(kEventName);
  if (iter == event_map.end())
    return false;

  const Event& event = *iter->second;
  CHECK_GE(1u, event.event_args.size());
  auto site_settings =
      api::developer_private::UserSiteSettings::FromValue(event.event_args[0]);
  if (!site_settings)
    return false;

  *settings = std::move(*site_settings);
  return true;
}

void AddUserSpecifiedSites(Profile* profile,
                           const std::string& hosts,
                           bool restricted) {
  auto function = base::MakeRefCounted<
      api::DeveloperPrivateAddUserSpecifiedSitesFunction>();
  std::string args = base::StringPrintf(
      R"([{"siteSet":"%s","hosts":%s}])",
      restricted ? "USER_RESTRICTED" : "USER_PERMITTED", hosts.c_str());
  EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile))
      << function->GetError();
}

void RemoveUserSpecifiedSites(Profile* profile,
                              const std::string& hosts,
                              bool restricted) {
  auto function = base::MakeRefCounted<
      api::DeveloperPrivateRemoveUserSpecifiedSitesFunction>();
  std::string args = base::StringPrintf(
      R"([{"siteSet":"%s","hosts":%s}])",
      restricted ? "USER_RESTRICTED" : "USER_PERMITTED", hosts.c_str());
  EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile))
      << function->GetError();
}

void AddExtensionAndGrantPermissions(Profile* profile,
                                     ExtensionService* service,
                                     const Extension& extension) {
  PermissionsUpdater updater(profile);
  updater.InitializePermissions(&extension);
  updater.GrantActivePermissions(&extension);
  service->AddExtension(&extension);
}

void RunAddHostPermission(Profile* profile,
                          const Extension& extension,
                          std::string_view host,
                          bool should_succeed,
                          const char* expected_error) {
  SCOPED_TRACE(host);
  auto function =
      base::MakeRefCounted<api::DeveloperPrivateAddHostPermissionFunction>();

  std::string args = base::StringPrintf(
      R"(["%s", "%s"])", extension.id().c_str(), std::string(host).c_str());
  if (should_succeed) {
    EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile))
        << function->GetError();
  } else {
    EXPECT_EQ(expected_error, api_test_utils::RunFunctionAndReturnError(
                                  function.get(), args, profile));
  }
}

void GetMatchingExtensionsForSite(
    Profile* profile,
    const std::string& site,
    std::vector<api::developer_private::MatchingExtensionInfo>* infos) {
  auto function = base::MakeRefCounted<
      api::DeveloperPrivateGetMatchingExtensionsForSiteFunction>();
  EXPECT_TRUE(api_test_utils::RunFunction(
      function.get(), base::StringPrintf(R"(["%s"])", site.c_str()), profile))
      << function->GetError();
  const base::Value::List* results = function->GetResultListForTest();
  ASSERT_EQ(1u, results->size());
  ASSERT_TRUE((*results)[0].is_list());

  infos->clear();
  for (const auto& value : (*results)[0].GetList()) {
    ASSERT_TRUE(value.is_dict());
    infos->push_back(std::move(
        *api::developer_private::MatchingExtensionInfo::FromValue(value)));
  }
}

auto MatchMatchingExtensionInfo(
    const ExtensionId& extension_id,
    const api::developer_private::HostAccess& host_access,
    bool can_request_all_sites) {
  return testing::AllOf(
      testing::Field(&api::developer_private::MatchingExtensionInfo::id,
                     extension_id),
      testing::Field(
          &api::developer_private::MatchingExtensionInfo::site_access,
          host_access),
      testing::Field(
          &api::developer_private::MatchingExtensionInfo::can_request_all_sites,
          can_request_all_sites));
}

api::developer_private::ExtensionSiteAccessUpdate CreateSiteAccessUpdate(
    const ExtensionId& id,
    api::developer_private::HostAccess access) {
  api::developer_private::ExtensionSiteAccessUpdate update;
  update.id = id;
  update.site_access = access;
  return update;
}

void UpdateSiteAccess(
    Profile* profile,
    const std::string& site,
    const std::vector<api::developer_private::ExtensionSiteAccessUpdate>&
        updates) {
  base::Value::List update_entries;
  update_entries.reserve(updates.size());
  for (const auto& update : updates) {
    update_entries.Append(update.ToValue());
  }
  std::string updates_arg;
  EXPECT_TRUE(base::JSONWriter::Write(update_entries, &updates_arg));

  scoped_refptr<ExtensionFunction> function =
      base::MakeRefCounted<api::DeveloperPrivateUpdateSiteAccessFunction>();
  EXPECT_TRUE(api_test_utils::RunFunction(
      function.get(),
      base::StringPrintf(R"(["%s", %s])", site.c_str(), updates_arg.c_str()),
      profile))
      << function->GetError();
}

}  // namespace

class DeveloperPrivateApiUnitTest : public ExtensionServiceTestWithInstall {
 public:
  DeveloperPrivateApiUnitTest(const DeveloperPrivateApiUnitTest&) = delete;
  DeveloperPrivateApiUnitTest& operator=(const DeveloperPrivateApiUnitTest&) =
      delete;

 protected:
  DeveloperPrivateApiUnitTest() {}
  ~DeveloperPrivateApiUnitTest() override {}

  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

  void AddMockExternalProvider(
      std::unique_ptr<ExternalProviderInterface> provider) {
    service()->AddProviderForTesting(std::move(provider));
  }

  // A wrapper around api_test_utils::RunFunction that runs with
  // the associated browser, no flags, and can take stack-allocated arguments.
  bool RunFunction(const scoped_refptr<ExtensionFunction>& function,
                   const base::Value::List& args);

  // Loads an unpacked extension that is backed by a real directory, allowing
  // it to be reloaded.
  const Extension* LoadUnpackedExtension();

  // Loads an extension with no real directory; this is faster, but means the
  // extension can't be reloaded.
  const Extension* LoadSimpleExtension();

  // Tests modifying the extension's configuration.
  void TestExtensionPrefSetting(const base::RepeatingCallback<bool()>& has_pref,
                                const std::string& key,
                                const ExtensionId& extension_id,
                                bool expected_default_value);

  testing::AssertionResult TestPackExtensionFunction(
      const base::Value::List& args,
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
      std::optional<api::developer_private::ProfileInfo>* profile_info);

  // Runs the API function to update host access for the given |extension| to
  // |new_access|.
  void RunUpdateHostAccess(const Extension& extension,
                           std::string_view new_access);

  virtual bool ProfileIsSupervised() const { return false; }

  Browser* browser() { return browser_.get(); }

  content::RenderProcessHost* render_process_host() const {
    return render_process_host_.get();
  }

 private:
  // This test does not create a root window. Because of this,
  // ScopedDisableRootChecking needs to be used (which disables the root window
  // check).
  test::ScopedDisableRootChecking disable_root_checking_;

  // The browser (and accompanying window).
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<content::RenderProcessHost> render_process_host_;

  std::vector<TestExtensionDir> test_extension_dirs_;
};

bool DeveloperPrivateApiUnitTest::RunFunction(
    const scoped_refptr<ExtensionFunction>& function,
    const base::Value::List& args) {
  return api_test_utils::RunFunction(
      function.get(), args.Clone(),
      std::make_unique<ExtensionFunctionDispatcher>(profile()),
      api_test_utils::FunctionMode::kNone);
}

const Extension* DeveloperPrivateApiUnitTest::LoadUnpackedExtension() {
  constexpr char kManifest[] =
      R"({
           "name": "foo",
           "version": "1.0",
           "manifest_version": 2,
           "permissions": ["*://*/*"]
         })";

  test_extension_dirs_.emplace_back();
  TestExtensionDir& dir = test_extension_dirs_.back();
  dir.WriteManifest(kManifest);

  ChromeTestExtensionLoader loader(profile());
  // The fact that unpacked extensions get file access by default is an
  // irrelevant detail to these tests. Disable it.
  loader.set_allow_file_access(false);

  return loader.LoadExtension(dir.UnpackedPath()).get();
}

const Extension* DeveloperPrivateApiUnitTest::LoadSimpleExtension() {
  const char kName[] = "extension name";
  const char kVersion[] = "1.0.0.1";
  ExtensionId id = crx_file::id_util::GenerateId(kName);
  auto manifest = base::Value::Dict()
                      .Set("name", kName)
                      .Set("version", kVersion)
                      .Set("manifest_version", 2)
                      .Set("description", "an extension");
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetLocation(mojom::ManifestLocation::kInternal)
          .SetID(id)
          .Build();
  service()->AddExtension(extension.get());
  return extension.get();
}

void DeveloperPrivateApiUnitTest::TestExtensionPrefSetting(
    const base::RepeatingCallback<bool()>& has_pref,
    const std::string& key,
    const ExtensionId& extension_id,
    bool expected_default_value) {
  EXPECT_EQ(expected_default_value, has_pref.Run()) << key;

  {
    base::Value::Dict parameters;
    parameters.Set("extensionId", extension_id);
    parameters.Set(key, true);

    base::Value::List args;
    args.Append(std::move(parameters));
    auto function = base::MakeRefCounted<
        api::DeveloperPrivateUpdateExtensionConfigurationFunction>();
    EXPECT_FALSE(RunFunction(function, args)) << key;
    EXPECT_EQ("This action requires a user gesture.", function->GetError());

    function = base::MakeRefCounted<
        api::DeveloperPrivateUpdateExtensionConfigurationFunction>();
    function->set_source_context_type(mojom::ContextType::kWebUi);
    EXPECT_TRUE(RunFunction(function, args)) << key;
    EXPECT_TRUE(has_pref.Run()) << key;
  }

  {
    base::Value::Dict parameters;
    parameters.Set("extensionId", extension_id);
    parameters.Set(key, false);

    base::Value::List args;
    args.Append(std::move(parameters));

    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
    auto function = base::MakeRefCounted<
        api::DeveloperPrivateUpdateExtensionConfigurationFunction>();
    EXPECT_TRUE(RunFunction(function, args)) << key;
    EXPECT_FALSE(has_pref.Run()) << key;
  }

  {
    base::Value::Dict parameters;
    parameters.Set("extensionId", extension_id);
    parameters.Set(key, true);

    base::Value::List args;
    args.Append(std::move(parameters));

    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
    auto function = base::MakeRefCounted<
        api::DeveloperPrivateUpdateExtensionConfigurationFunction>();
    EXPECT_TRUE(RunFunction(function, args)) << key;
    EXPECT_TRUE(has_pref.Run()) << key;
  }
}

testing::AssertionResult DeveloperPrivateApiUnitTest::TestPackExtensionFunction(
    const base::Value::List& args,
    api::developer_private::PackStatus expected_status,
    int expected_flags) {
  auto function =
      base::MakeRefCounted<api::DeveloperPrivatePackDirectoryFunction>();
  if (!RunFunction(function, args))
    return testing::AssertionFailure() << "Could not run function.";

  // Extract the result. We don't have to test this here, since it's verified as
  // part of the general extension api system.
  const base::Value& response_value = (*function->GetResultListForTest())[0];
  std::optional<api::developer_private::PackDirectoryResponse> response =
      api::developer_private::PackDirectoryResponse::FromValue(response_value);
  CHECK(response);

  if (response->status != expected_status) {
    return testing::AssertionFailure()
           << "Expected status: "
           << api::developer_private::ToString(expected_status)
           << ", found status: "
           << api::developer_private::ToString(response->status)
           << ", message: " << response->message;
  }

  if (response->override_flags != expected_flags) {
    return testing::AssertionFailure() << "Expected flags: " <<
        expected_flags << ", found flags: " << response->override_flags;
  }

  return testing::AssertionSuccess();
}

void DeveloperPrivateApiUnitTest::UpdateProfileConfigurationDevMode(
    bool dev_mode) {
  auto function = base::MakeRefCounted<
      api::DeveloperPrivateUpdateProfileConfigurationFunction>();
  base::Value::List args = base::Value::List().Append(
      base::Value::Dict().Set("inDeveloperMode", dev_mode));
  EXPECT_TRUE(RunFunction(function, args)) << function->GetError();
}

void DeveloperPrivateApiUnitTest::GetProfileConfiguration(
    std::optional<api::developer_private::ProfileInfo>* profile_info) {
  auto function = base::MakeRefCounted<
      api::DeveloperPrivateGetProfileConfigurationFunction>();
  base::Value::List args;
  EXPECT_TRUE(RunFunction(function, args)) << function->GetError();

  ASSERT_TRUE(function->GetResultListForTest());
  ASSERT_EQ(1u, function->GetResultListForTest()->size());
  const base::Value& response_value = (*function->GetResultListForTest())[0];
  *profile_info =
      api::developer_private::ProfileInfo::FromValue(response_value);
}

void DeveloperPrivateApiUnitTest::RunUpdateHostAccess(
    const Extension& extension,
    std::string_view new_access) {
  SCOPED_TRACE(new_access);
  ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
  auto function = base::MakeRefCounted<
      api::DeveloperPrivateUpdateExtensionConfigurationFunction>();
  std::string args =
      base::StringPrintf(R"([{"extensionId": "%s", "hostAccess": "%s"}])",
                         extension.id().c_str(), new_access.data());
  EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile()))
      << function->GetError();
}

void DeveloperPrivateApiUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();

  ExtensionServiceInitParams init_params;
  init_params.profile_is_supervised = ProfileIsSupervised();
  InitializeExtensionService(std::move(init_params));
  extension_action_test_util::CreateToolbarModelForProfile(profile());

  browser_window_ = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = browser_window_.get();
  browser_.reset(Browser::Create(params));

  // Allow the API to be created.
  EventRouterFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildEventRouter));

  DeveloperPrivateAPI::GetFactoryInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildAPI));

  // Loading unpacked extensions through the developerPrivate API requires
  // developer mode to be enabled.
  profile()->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);
  render_process_host_ =
      std::make_unique<content::MockRenderProcessHost>(profile());
}

void DeveloperPrivateApiUnitTest::TearDown() {
  test_extension_dirs_.clear();
  browser_.reset();
  browser_window_.reset();
  render_process_host_.reset();
  ExtensionServiceTestBase::TearDown();
}

// Test developerPrivate.updateExtensionConfiguration.
TEST_F(DeveloperPrivateApiUnitTest,
       DeveloperPrivateUpdateExtensionConfiguration) {
  // Sadly, we need a "real" directory here, because toggling prefs causes
  // a reload (which needs a path).
  const Extension* extension = LoadUnpackedExtension();
  const ExtensionId& id = extension->id();

  ScriptingPermissionsModifier(profile(), base::WrapRefCounted(extension))
      .SetWithholdHostPermissions(true);

  // Test pinning to toolbar first as this needs the extension to be enabled.
  // The other pref settings tested below may disable the extension so it will
  // not have an action in the toolbar.
  auto pinned_to_toolbar = [&]() {
    ToolbarActionsModel* toolbar_actions_model =
        ToolbarActionsModel::Get(profile());
    return toolbar_actions_model->HasAction(id) &&
           toolbar_actions_model->IsActionPinned(id);
  };
  TestExtensionPrefSetting(base::BindLambdaForTesting(pinned_to_toolbar),
                           "pinnedToToolbar", id,
                           /*expected_default_value=*/false);

  TestExtensionPrefSetting(
      base::BindRepeating(&HasPrefsPermission, &util::IsIncognitoEnabled,
                          profile(), id),
      "incognitoAccess", id, /*expected_default_value=*/false);
  TestExtensionPrefSetting(
      base::BindRepeating(&HasPrefsPermission, &util::AllowFileAccess,
                          profile(), id),
      "fileAccess", id, /*expected_default_value=*/false);

  SitePermissionsHelper helper(profile());
  TestExtensionPrefSetting(
      base::BindRepeating(&SitePermissionsHelper::ShowAccessRequestsInToolbar,
                          base::Unretained(&helper), id),
      "showAccessRequestsInToolbar", id, /*expected_default_value=*/true);

  // Check to ensure the `kPrefAcknowledgeSafetyCheckWarningReason` is not
  // set yet.
  int warning_reason = 0;
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());
  EXPECT_FALSE(extension_prefs->ReadPrefAsInteger(
      id, extensions::kPrefAcknowledgeSafetyCheckWarningReason,
      &warning_reason));

  // Test `acknowledgeSafetyCheckWarningReason` pref.
  base::Value::List args;
  args.Append(base::Value::Dict()
                  .Set("extensionId", id)
                  .Set("acknowledgeSafetyCheckWarningReason", "MALWARE"));

  ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
  auto function = base::MakeRefCounted<
      api::DeveloperPrivateUpdateExtensionConfigurationFunction>();
  EXPECT_TRUE(RunFunction(function, args));

  extension_prefs->ReadPrefAsInteger(
      id, extensions::kPrefAcknowledgeSafetyCheckWarningReason,
      &warning_reason);
  api::developer_private::SafetyCheckWarningReason warning_reason_enum =
      static_cast<api::developer_private::SafetyCheckWarningReason>(
          warning_reason);
  EXPECT_EQ(warning_reason_enum,
            api::developer_private::SafetyCheckWarningReason::kMalware);
}

// Test developerPrivate.reload.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateReload) {
  const Extension* extension = LoadUnpackedExtension();
  ExtensionId extension_id = extension->id();
  auto function = base::MakeRefCounted<api::DeveloperPrivateReloadFunction>();
  base::Value::List reload_args;
  reload_args.Append(extension_id);

  TestExtensionRegistryObserver registry_observer(registry());
  EXPECT_TRUE(RunFunction(function, reload_args));
  scoped_refptr<const Extension> unloaded_extension =
      registry_observer.WaitForExtensionUnloaded();
  EXPECT_EQ(extension, unloaded_extension);
  scoped_refptr<const Extension> reloaded_extension =
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
  base::Value::List pack_args;
  pack_args.Append(temp_root_path.AsUTF8Unsafe());
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args, api::developer_private::PackStatus::kSuccess, 0));

  // Should have created crx file and pem file.
  EXPECT_TRUE(base::PathExists(crx_path));
  EXPECT_TRUE(base::PathExists(pem_path));

  // Deliberately don't cleanup the files, and append the pem path.
  pack_args.Append(pem_path.AsUTF8Unsafe());

  // Try to pack again - we should get a warning abot overwriting the crx.
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args, api::developer_private::PackStatus::kWarning,
      ExtensionCreator::kOverwriteCRX));

  // Try to pack again, with the overwrite flag; this should succeed.
  pack_args.Append(ExtensionCreator::kOverwriteCRX);
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args, api::developer_private::PackStatus::kSuccess, 0));

  // Try to pack a final time when omitting (an existing) pem file. We should
  // get an error.
  base::DeleteFile(crx_path);
  // Remove the pem key and flags arguments.
  pack_args.erase(pack_args.begin() + 1, pack_args.begin() + 3);
  EXPECT_TRUE(TestPackExtensionFunction(
      pack_args, api::developer_private::PackStatus::kError, 0));
}

// Test developerPrivate.choosePath.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateChoosePath) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  base::FilePath expected_dir_path =
      data_dir().AppendASCII("simple_with_popup");
  base::FilePath expected_file_path =
      data_dir().AppendASCII("simple_with_popup.pem");

  // Try selecting a directory.
  auto function =
      base::MakeRefCounted<api::DeveloperPrivateChoosePathFunction>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  function->set_accept_dialog_for_testing(true);
  function->set_selected_file_for_testing(
      ui::SelectedFileInfo(expected_dir_path));
  base::Value::List choose_args;
  choose_args.Append("FOLDER");
  choose_args.Append("LOAD");
  EXPECT_TRUE(RunFunction(function, choose_args)) << function->GetError();

  // Verify directory was properly chosen.
  std::string path;
  const base::Value::List* result_list = function->GetResultListForTest();
  ASSERT_TRUE(result_list);
  ASSERT_GT(result_list->size(), 0u);
  ASSERT_TRUE((*result_list)[0].is_string());
  path = (*result_list)[0].GetString();
  EXPECT_EQ(path, expected_dir_path.AsUTF8Unsafe());

  // Try selecting a pem file.
  function = base::MakeRefCounted<api::DeveloperPrivateChoosePathFunction>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  function->set_accept_dialog_for_testing(true);
  function->set_selected_file_for_testing(
      ui::SelectedFileInfo(expected_file_path));
  choose_args.clear();
  choose_args.Append("FILE");
  choose_args.Append("PEM");
  EXPECT_TRUE(RunFunction(function, choose_args)) << function->GetError();

  // Verify pem file was properly chosen.
  result_list = function->GetResultListForTest();
  ASSERT_TRUE(result_list);
  ASSERT_GT(result_list->size(), 0u);
  ASSERT_TRUE((*result_list)[0].is_string());
  path = (*result_list)[0].GetString();
  EXPECT_EQ(path, expected_file_path.AsUTF8Unsafe());

  // Try canceling the file dialog.
  function = base::MakeRefCounted<api::DeveloperPrivateChoosePathFunction>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  function->set_accept_dialog_for_testing(false);
  EXPECT_FALSE(RunFunction(function, choose_args));

  // Verify function returns an error.
  EXPECT_EQ(std::string("File selection was canceled."), function->GetError());
}

// Test developerPrivate.loadUnpacked.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateLoadUnpacked) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  ExtensionIdSet current_ids = registry()->enabled_extensions().GetIDs();

  // Try loading an extension and canceling the dialog.
  auto function =
      base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
  function->set_accept_dialog_for_testing(false);
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  EXPECT_FALSE(RunFunction(function, base::Value::List()));

  // Function should fail and no new extensions are installed.
  // NOTE: This isn't really an error, but we kept it like this for backward
  // compatibility.
  EXPECT_EQ("File selection was canceled.", function->GetError());
  EXPECT_EQ(0u, base::STLSetDifference<ExtensionIdSet>(
                    registry()->enabled_extensions().GetIDs(), current_ids)
                    .size());

  // Try loading a good extension and accepting the dialog.
  function = base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
  base::FilePath path = data_dir().AppendASCII("simple_with_popup");
  function->set_accept_dialog_for_testing(true);
  function->set_selected_file_for_testing(ui::SelectedFileInfo(path));
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());

  // Function should succeed and extension is added.
  EXPECT_TRUE(RunFunction(function, base::Value::List()))
      << function->GetError();
  ExtensionIdSet id_difference = base::STLSetDifference<ExtensionIdSet>(
      registry()->enabled_extensions().GetIDs(), current_ids);
  ASSERT_EQ(1u, id_difference.size());
  // The new extension should have the same path.
  EXPECT_EQ(
      path,
      registry()->enabled_extensions().GetByID(*id_difference.begin())->path());

  // Try loading a bad extension and accepting the dialog.
  function = base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
  path = data_dir().AppendASCII("empty_manifest");
  function->set_accept_dialog_for_testing(true);
  function->set_selected_file_for_testing(ui::SelectedFileInfo(path));
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  base::Value::List unpacked_args;
  base::Value::Dict options;
  options.Set("failQuietly", true);
  unpacked_args.Append(std::move(options));
  current_ids = registry()->enabled_extensions().GetIDs();
  EXPECT_FALSE(RunFunction(function, unpacked_args));

  // Function should fail and no new extensions are installed.
  EXPECT_EQ(manifest_errors::kManifestUnreadable, function->GetError());
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

    auto function =
        base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
    function->set_accept_dialog_for_testing(true);
    function->set_selected_file_for_testing(ui::SelectedFileInfo(path));
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
    std::optional<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(),
            "[{\"failQuietly\": true, \"populateError\": true}]", profile());

    // The loadError result should be populated.
    ASSERT_TRUE(result);
    std::optional<api::developer_private::LoadError> error =
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

    auto function =
        base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
    function->set_accept_dialog_for_testing(true);
    function->set_selected_file_for_testing(ui::SelectedFileInfo(path));
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
    std::optional<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(),
            "[{\"failQuietly\": true, \"populateError\": true}]", profile());
    // The load error should be populated.
    ASSERT_TRUE(result);
    std::optional<api::developer_private::LoadError> error =
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

    auto function =
        base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
    function->set_accept_dialog_for_testing(true);
    function->set_selected_file_for_testing(ui::SelectedFileInfo(path));
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
    std::optional<base::Value> result =
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

  DeveloperPrivateAPI::UnpackedRetryId retry_guid;
  {
    // Trying to load the extension should result in a load error with the
    // retry id populated.
    auto function =
        base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
    function->set_accept_dialog_for_testing(true);
    function->set_selected_file_for_testing(ui::SelectedFileInfo(path));
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
    std::optional<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(),
            "[{\"failQuietly\": true, \"populateError\": true}]", profile());
    ASSERT_TRUE(result);
    std::optional<api::developer_private::LoadError> error =
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
    auto function =
        base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
    function->set_accept_dialog_for_testing(true);
    function->set_selected_file_for_testing(ui::SelectedFileInfo(path));
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
    std::optional<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(),
            "[{\"failQuietly\": true, \"populateError\": true}]", profile());
    ASSERT_TRUE(result);
    std::optional<api::developer_private::LoadError> error =
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

    auto function =
        base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
    function->set_accept_dialog_for_testing(true);
    function->set_selected_file_for_testing(ui::SelectedFileInfo(second_path));
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
    std::optional<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(),
            "[{\"failQuietly\": true, \"populateError\": true}]", profile());
    // The loadError result should be populated.
    ASSERT_TRUE(result);
    std::optional<api::developer_private::LoadError> error =
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

  {
    // Try reloading the extension by supplying the retry id. It should succeed.
    auto function =
        base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
    function->set_accept_dialog_for_testing(true);
    function->set_selected_file_for_testing(ui::SelectedFileInfo(empty_path));
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
    TestExtensionRegistryObserver observer(registry());
    api_test_utils::RunFunction(function.get(),
                                base::StringPrintf("[{\"failQuietly\": true,"
                                                   "\"populateError\": true,"
                                                   "\"retryGuid\": \"%s\"}]",
                                                   retry_guid.c_str()),
                                profile());
    scoped_refptr<const Extension> extension =
        observer.WaitForExtensionLoaded();
    ASSERT_TRUE(extension);
    EXPECT_EQ(extension->path(), path);
  }

  {
    // Try supplying an invalid retry id. It should fail with an error.
    auto function =
        base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
    function->set_accept_dialog_for_testing(true);
    function->set_selected_file_for_testing(ui::SelectedFileInfo(empty_path));
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
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
          : expected_path_(expected_path) {
        observation_.Observe(registry);
      }

      UnloadedRegistryObserver(const UnloadedRegistryObserver&) = delete;
      UnloadedRegistryObserver& operator=(const UnloadedRegistryObserver&) =
          delete;

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
      base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
          observation_{this};
    };

    UnloadedRegistryObserver unload_observer(path, registry());
    auto function =
        base::MakeRefCounted<api::DeveloperPrivateReloadFunction>();
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
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
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
    std::optional<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), reload_args, profile());
    ASSERT_TRUE(result);
    std::optional<api::developer_private::LoadError> error =
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
    function->set_accept_dialog_for_testing(true);
    function->set_selected_file_for_testing(ui::SelectedFileInfo(path));
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
    TestExtensionRegistryObserver observer(registry());
    std::string args =
        base::StringPrintf(R"([{"failQuietly": true, "populateError": true,
                                "retryGuid": "%s"}])",
                           retry_guid.c_str());
    api_test_utils::RunFunction(function.get(), args, profile());
    scoped_refptr<const Extension> reloaded_extension =
        observer.WaitForExtensionLoaded();
    ASSERT_TRUE(reloaded_extension);
    EXPECT_EQ(reloaded_extension->path(), path);
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
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
    api_test_utils::RunFunction(function.get(), "[]", profile());
  }

  constexpr char kLoadUnpackedArgs[] =
      R"([{"failQuietly": true,
           "populateError": true,
           "useDraggedPath": true}])";

  {
    // Try reloading the extension by supplying the retry id. It should succeed.
    auto function =
        base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
    // Set file picker dialog to be accepted with an invalid path (the dialog
    // should be skipped if we supply a retry id).
    base::FilePath empty_path;
    function->set_accept_dialog_for_testing(true);
    function->set_selected_file_for_testing(ui::SelectedFileInfo(empty_path));
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());

    TestExtensionRegistryObserver observer(registry());
    api_test_utils::RunFunction(function.get(), kLoadUnpackedArgs, profile());
    scoped_refptr<const Extension> extension =
        observer.WaitForExtensionLoaded();
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
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
    std::optional<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(function.get(), "[]",
                                                         profile());
  }

  {
    // Trying to load the bad extension (the path points to the manifest, not
    // the directory) should result in a load error.
    auto function =
        base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
    // Set file picker dialog to be accepted with an invalid path (the dialog
    // should be skipped if we supply a retry id).
    base::FilePath empty_path;
    function->set_accept_dialog_for_testing(true);
    function->set_selected_file_for_testing(ui::SelectedFileInfo(empty_path));
    TestExtensionRegistryObserver observer(registry());
    std::optional<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), kLoadUnpackedArgs, profile());
    ASSERT_TRUE(result);
    EXPECT_TRUE(api::developer_private::LoadError::FromValue(*result));
  }

  // Cleanup.
  api::DeveloperPrivateNotifyDragInstallInProgressFunction::
      SetDropPathForTesting(nullptr);
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
  properties.manifest_key = "name";

  auto function =
      base::MakeRefCounted<api::DeveloperPrivateRequestFileSourceFunction>();
  base::Value::List file_source_args;
  file_source_args.Append(properties.ToValue());
  EXPECT_TRUE(RunFunction(function, file_source_args)) << function->GetError();

  const base::Value& response_value = (*function->GetResultListForTest())[0];
  std::optional<api::developer_private::RequestFileSourceResponse> response =
      api::developer_private::RequestFileSourceResponse::FromValue(
          response_value);
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
  auto function =
      base::MakeRefCounted<api::DeveloperPrivateGetExtensionsInfoFunction>();
  EXPECT_TRUE(RunFunction(function, base::Value::List()))
      << function->GetError();
  const base::Value::List* results = function->GetResultListForTest();
  ASSERT_EQ(1u, results->size());
  ASSERT_TRUE((*results)[0].is_list());
  const base::Value::List& list = (*results)[0].GetList();
  ASSERT_EQ(1u, list.size());
  std::optional<api::developer_private::ExtensionInfo> info =
      api::developer_private::ExtensionInfo::FromValue(list[0]);
  ASSERT_TRUE(info);
}

// Test developerPrivate.deleteExtensionErrors.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateDeleteExtensionErrors) {
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
      api::developer_private::ErrorType::kManifest);
  base::Value::List args =
      base::Value::List().Append(base::Value::Dict()
                                     .Set("extensionId", extension->id())
                                     .Set("type", type_string));
  auto function = base::MakeRefCounted<
      api::DeveloperPrivateDeleteExtensionErrorsFunction>();
  EXPECT_TRUE(RunFunction(function, args)) << function->GetError();
  // Two errors should remain.
  const ErrorList& error_list =
      error_console->GetErrorsForExtension(extension->id());
  ASSERT_EQ(2u, error_list.size());

  // Next remove errors by id.
  int error_id = error_list[0]->id();
  args = base::Value::List().Append(
      base::Value::Dict()
          .Set("extensionId", extension->id())
          .Set("errorIds", base::Value::List().Append(error_id)));
  function = base::MakeRefCounted<
      api::DeveloperPrivateDeleteExtensionErrorsFunction>();
  EXPECT_TRUE(RunFunction(function, args)) << function->GetError();
  // And then there was one.
  EXPECT_EQ(1u, error_console->GetErrorsForExtension(extension->id()).size());

  // Finally remove all errors for the extension.
  args = base::Value::List().Append(
      base::Value::Dict().Set("extensionId", extension->id()));
  function = base::MakeRefCounted<
      api::DeveloperPrivateDeleteExtensionErrorsFunction>();
  EXPECT_TRUE(RunFunction(function, args)) << function->GetError();
  // No more errors!
  EXPECT_TRUE(error_console->GetErrorsForExtension(extension->id()).empty());
}

// Tests that developerPrivate.repair does not succeed for a non-corrupted
// extension.
TEST_F(DeveloperPrivateApiUnitTest, RepairNotBrokenExtension) {
  base::FilePath extension_path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(extension_path, INSTALL_NEW);

  // Attempt to repair the good extension, expect failure.
  base::Value::List args = base::Value::List().Append(extension->id());
  auto function =
      base::MakeRefCounted<api::DeveloperPrivateRepairExtensionFunction>();
  EXPECT_FALSE(RunFunction(function, args));
  EXPECT_EQ("Cannot repair a healthy extension.", function->GetError());
}

// Tests that developerPrivate.private cannot repair a policy-installed
// extension.
// Regression test for https://crbug.com/577959.
TEST_F(DeveloperPrivateApiUnitTest, RepairPolicyExtension) {
  ExtensionId extension_id(kGoodCrx);

  // Set up a mock provider with a policy extension.
  std::unique_ptr<MockExternalProvider> mock_provider =
      std::make_unique<MockExternalProvider>(
          service(), mojom::ManifestLocation::kExternalPolicyDownload);
  MockExternalProvider* mock_provider_ptr = mock_provider.get();
  AddMockExternalProvider(std::move(mock_provider));
  mock_provider_ptr->UpdateOrAddExtension(extension_id, "1.0.0.0",
                                          data_dir().AppendASCII("good.crx"));
  // Reloading extensions should find our externally registered extension
  // and install it.
  {
    TestExtensionRegistryObserver observer(registry());
    service()->CheckForExternalUpdates();
    EXPECT_EQ(extension_id, observer.WaitForExtensionLoaded()->id());
  }

  // Attempt to repair the good extension, expect failure.
  base::Value::List args = base::Value::List().Append(extension_id);
  auto function =
      base::MakeRefCounted<api::DeveloperPrivateRepairExtensionFunction>();
  EXPECT_FALSE(RunFunction(function, args));
  EXPECT_EQ("Cannot repair a healthy extension.", function->GetError());

  // Corrupt the extension, still expect repair failure because this is a
  // policy extension.
  service()->DisableExtension(extension_id, disable_reason::DISABLE_CORRUPTED);
  args = base::Value::List().Append(extension_id);
  function =
      base::MakeRefCounted<api::DeveloperPrivateRepairExtensionFunction>();
  EXPECT_FALSE(RunFunction(function, args));
  EXPECT_EQ("Cannot repair a policy-installed extension.",
            function->GetError());
}

// Tests that developerPrivate.repair does not succeed for an extension not from
// the Chrome Web Store.
TEST_F(DeveloperPrivateApiUnitTest, RepairNonCWSExtension) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  base::FilePath extension_path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(extension_path, INSTALL_NEW);

  // Corrupt the extension, still expect repair failure because `good.crx` does
  // not update from the web store.
  service()->DisableExtension(extension->id(),
                              disable_reason::DISABLE_CORRUPTED);

  base::Value::List args = base::Value::List().Append(extension->id());
  auto function =
      base::MakeRefCounted<api::DeveloperPrivateRepairExtensionFunction>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  EXPECT_FALSE(RunFunction(function, args));
  EXPECT_EQ(
      "Cannot repair an extension that is not installed from the Chrome Web "
      "Store.",
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

  std::optional<api::developer_private::ProfileInfo> profile_info;
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
    std::optional<api::developer_private::ProfileInfo> profile_info;
    ASSERT_NO_FATAL_FAILURE(GetProfileConfiguration(&profile_info));
    EXPECT_FALSE(profile_info->in_developer_mode);
    EXPECT_FALSE(profile_info->is_developer_mode_controlled_by_policy);
  }

  UpdateProfileConfigurationDevMode(true);
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kExtensionsUIDeveloperMode));
  {
    std::optional<api::developer_private::ProfileInfo> profile_info;
    ASSERT_NO_FATAL_FAILURE(GetProfileConfiguration(&profile_info));
    EXPECT_TRUE(profile_info->in_developer_mode);
    EXPECT_FALSE(profile_info->is_developer_mode_controlled_by_policy);
  }
}

TEST_F(DeveloperPrivateApiUnitTest, LoadUnpackedFailsWithoutDevMode) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  PrefService* prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kExtensionsUIDeveloperMode, false);

  auto function =
      base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());

  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), "[]", profile());
  EXPECT_THAT(error, testing::HasSubstr("developer mode"));
  prefs->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);
}

TEST_F(DeveloperPrivateApiUnitTest, LoadUnpackedFailsWithBlocklistingPolicy) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  {
    ExtensionManagementPrefUpdater<sync_preferences::TestingPrefServiceSyncable>
        pref_updater(testing_profile()->GetTestingPrefService());
    pref_updater.SetBlocklistedByDefault(true);
  }

  auto* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(browser_context());
  EXPECT_TRUE(extension_management->BlocklistedByDefault());
  EXPECT_FALSE(extension_management->HasAllowlistedExtension());

  auto info = DeveloperPrivateAPI::CreateProfileInfo(testing_profile());
  EXPECT_FALSE(info->can_load_unpacked);

  auto function =
      base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());

  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), "[]", profile());
  EXPECT_THAT(error, testing::HasSubstr("policy"));
}

TEST_F(DeveloperPrivateApiUnitTest,
       LoadUnpackedWorksWithBlocklistingPolicyAlongAllowlistingPolicy) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  {
    ExtensionManagementPrefUpdater<sync_preferences::TestingPrefServiceSyncable>
        pref_updater(testing_profile()->GetTestingPrefService());
    pref_updater.SetBlocklistedByDefault(true);
    pref_updater.SetIndividualExtensionInstallationAllowed(kGoodCrx, true);
  }

  EXPECT_TRUE(
      ExtensionManagementFactory::GetForBrowserContext(browser_context())
          ->BlocklistedByDefault());

  EXPECT_TRUE(
      ExtensionManagementFactory::GetForBrowserContext(browser_context())
          ->HasAllowlistedExtension());

  auto info = DeveloperPrivateAPI::CreateProfileInfo(testing_profile());

  EXPECT_TRUE(info->can_load_unpacked);
}

TEST_F(DeveloperPrivateApiUnitTest, InstallDroppedFileNoDraggedPath) {
  base::AutoReset<bool> disable_ui =
      ExtensionInstallUI::disable_ui_for_tests(true);
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  auto function =
      base::MakeRefCounted<api::DeveloperPrivateInstallDroppedFileFunction>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());

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
  base::AutoReset<bool> disable_ui =
      ExtensionInstallUI::disable_ui_for_tests(true);
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  DeveloperPrivateAPI::Get(profile())->SetDraggedPath(web_contents.get(),
                                                      crx_path);

  auto function =
      base::MakeRefCounted<api::DeveloperPrivateInstallDroppedFileFunction>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());

  TestExtensionRegistryObserver observer(registry());
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), "[]", profile()))
      << function->GetError();
  scoped_refptr<const Extension> extension =
      observer.WaitForExtensionInstalled();
  ASSERT_TRUE(extension);
  EXPECT_EQ("foo", extension->name());
}

TEST_F(DeveloperPrivateApiUnitTest, InstallDroppedFileUserScript) {
  base::FilePath script_path =
      data_dir().AppendASCII("user_script_basic.user.js");
  base::AutoReset<bool> disable_ui =
      ExtensionInstallUI::disable_ui_for_tests(true);
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  DeveloperPrivateAPI::Get(profile())->SetDraggedPath(web_contents.get(),
                                                      script_path);

  auto function =
      base::MakeRefCounted<api::DeveloperPrivateInstallDroppedFileFunction>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());

  TestExtensionRegistryObserver observer(registry());
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), "[]", profile()))
      << function->GetError();
  scoped_refptr<const Extension> extension =
      observer.WaitForExtensionInstalled();
  ASSERT_TRUE(extension);
  EXPECT_EQ("My user script", extension->name());
}

TEST_F(DeveloperPrivateApiUnitTest, GrantHostPermission) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test").AddHostPermission("<all_urls>").Build();
  service()->AddExtension(extension.get());

  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  EXPECT_FALSE(permissions_manager->HasWithheldHostPermissions(*extension));

  ScriptingPermissionsModifier modifier(profile(), extension.get());
  modifier.SetWithholdHostPermissions(true);

  const GURL kExampleCom("https://example.com/");
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension, kExampleCom));
  RunAddHostPermission(profile(), *extension, "https://example.com/*",
                       /*should_succeed=*/true, nullptr);
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, kExampleCom));

  const GURL kGoogleCom("https://google.com");
  const GURL kMapsGoogleCom("https://maps.google.com/");
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension, kGoogleCom));
  EXPECT_FALSE(permissions_manager->HasGrantedHostPermission(*extension,
                                                             kMapsGoogleCom));
  RunAddHostPermission(profile(), *extension, "https://*.google.com/*",
                       /*should_succeed=*/true, nullptr);
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, kGoogleCom));
  EXPECT_TRUE(permissions_manager->HasGrantedHostPermission(*extension,
                                                            kMapsGoogleCom));

  RunAddHostPermission(profile(), *extension, kInvalidHost,
                       /*should_succeed=*/false, kInvalidHostError);
  // Path of the pattern must exactly match "/*".
  RunAddHostPermission(profile(), *extension, "https://example.com/",
                       /*should_succeed=*/false, kInvalidHostError);
  RunAddHostPermission(profile(), *extension, "https://example.com/foobar",
                       /*should_succeed=*/false, kInvalidHostError);
  RunAddHostPermission(profile(), *extension, "https://example.com/#foobar",
                       /*should_succeed=*/false, kInvalidHostError);
  RunAddHostPermission(profile(), *extension, "https://example.com/*foobar",
                       /*should_succeed=*/false, kInvalidHostError);

  // Cannot grant chrome:-scheme URLs.
  GURL chrome_host("chrome://settings/*");
  RunAddHostPermission(profile(), *extension, chrome_host.spec(),
                       /*should_succeed=*/false, kInvalidHostError);

  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension, chrome_host));
}

TEST_F(DeveloperPrivateApiUnitTest, RemoveHostPermission) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test").AddHostPermission("<all_urls>").Build();
  service()->AddExtension(extension.get());

  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  EXPECT_FALSE(permissions_manager->HasWithheldHostPermissions(*extension));

  ScriptingPermissionsModifier modifier(profile(), extension.get());
  modifier.SetWithholdHostPermissions(true);

  auto run_remove_host_permission = [this, extension](
                                        std::string_view host,
                                        bool should_succeed,
                                        const char* expected_error) {
    SCOPED_TRACE(host);
    auto function = base::MakeRefCounted<
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
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, kExampleCom));

  // Path of the pattern must exactly match "/*".
  run_remove_host_permission("https://example.com/", false, kInvalidHostError);
  run_remove_host_permission("https://example.com/foobar", false,
                             kInvalidHostError);
  run_remove_host_permission("https://example.com/#foobar", false,
                             kInvalidHostError);
  run_remove_host_permission("https://example.com/*foobar", false,
                             kInvalidHostError);
  run_remove_host_permission(kInvalidHost, false, kInvalidHostError);
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, kExampleCom));

  run_remove_host_permission("https://example.com/*", true, nullptr);
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension, kExampleCom));

  URLPattern new_pattern(Extension::kValidHostPermissionSchemes,
                         "https://*.google.com/*");
  permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
      profile(), *extension,
      PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                    URLPatternSet({new_pattern}), URLPatternSet()));

  const GURL kGoogleCom("https://google.com/");
  const GURL kMapsGoogleCom("https://maps.google.com/");
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, kGoogleCom));
  EXPECT_TRUE(permissions_manager->HasGrantedHostPermission(*extension,
                                                            kMapsGoogleCom));

  run_remove_host_permission("https://*.google.com/*", true, nullptr);
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension, kGoogleCom));
  EXPECT_FALSE(permissions_manager->HasGrantedHostPermission(*extension,
                                                             kMapsGoogleCom));
}

TEST_F(DeveloperPrivateApiUnitTest, UpdateHostAccess) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test").AddHostPermission("<all_urls>").Build();
  service()->AddExtension(extension.get());

  PermissionsManager* permissions_manager =
      PermissionsManager::Get(browser()->profile());
  EXPECT_FALSE(permissions_manager->HasWithheldHostPermissions(*extension));

  RunUpdateHostAccess(*extension, "ON_CLICK");
  EXPECT_TRUE(permissions_manager->HasWithheldHostPermissions(*extension));

  RunUpdateHostAccess(*extension, "ON_ALL_SITES");
  EXPECT_FALSE(permissions_manager->HasWithheldHostPermissions(*extension));

  RunUpdateHostAccess(*extension, "ON_SPECIFIC_SITES");
  EXPECT_TRUE(permissions_manager->HasWithheldHostPermissions(*extension));
}

TEST_F(DeveloperPrivateApiUnitTest,
       UpdateHostAccess_SpecificSitesRemovedOnTransitionToOnClick) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test").AddHostPermission("<all_urls>").Build();
  service()->AddExtension(extension.get());
  ScriptingPermissionsModifier modifier(profile(), extension.get());
  modifier.SetWithholdHostPermissions(true);

  const GURL example_com("https://example.com");
  modifier.GrantHostPermission(example_com);
  PermissionsManager* permissions_manager =
      PermissionsManager::Get(browser()->profile());

  RunUpdateHostAccess(*extension, "ON_SPECIFIC_SITES");
  EXPECT_TRUE(permissions_manager->HasWithheldHostPermissions(*extension));
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, example_com));

  RunUpdateHostAccess(*extension, "ON_CLICK");
  EXPECT_TRUE(permissions_manager->HasWithheldHostPermissions(*extension));
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension, example_com));

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
  EXPECT_TRUE(permissions_manager->HasWithheldHostPermissions(*extension));
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension, example_com));
}

TEST_F(DeveloperPrivateApiUnitTest,
       UpdateHostAccess_SpecificSitesRemovedOnTransitionToAllSites) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test").AddHostPermission("<all_urls>").Build();
  service()->AddExtension(extension.get());
  ScriptingPermissionsModifier modifier(profile(), extension.get());
  modifier.SetWithholdHostPermissions(true);

  PermissionsManager* permissions_manager =
      PermissionsManager::Get(browser()->profile());
  const GURL example_com("https://example.com");

  RunUpdateHostAccess(*extension, "ON_SPECIFIC_SITES");
  modifier.GrantHostPermission(example_com);
  EXPECT_TRUE(permissions_manager->HasWithheldHostPermissions(*extension));
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, example_com));

  RunUpdateHostAccess(*extension, "ON_ALL_SITES");
  EXPECT_FALSE(permissions_manager->HasWithheldHostPermissions(*extension));
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, example_com));

  RunUpdateHostAccess(*extension, "ON_SPECIFIC_SITES");
  EXPECT_TRUE(permissions_manager->HasWithheldHostPermissions(*extension));
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension, example_com));
}

TEST_F(DeveloperPrivateApiUnitTest,
       UpdateHostAccess_BroadPermissionsRemovedOnTransitionToSpecificSites) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test").AddHostPermission("<all_urls>").Build();
  service()->AddExtension(extension.get());
  ScriptingPermissionsModifier modifier(profile(), extension.get());
  modifier.SetWithholdHostPermissions(true);

  const GURL kGoogleCom("https://google.com/");
  const GURL kChromiumCom("https://chromium.com");

  // Request <all_urls> and google.com so they are both in the runtime granted
  // list. We use the util function to specifically add the <all_urls> pattern
  // here, similar to if it was requested through the chrome.permissions.request
  // API.
  URLPattern all_url_pattern(Extension::kValidHostPermissionSchemes,
                             "<all_urls>");
  permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
      profile(), *extension,
      PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                    URLPatternSet({all_url_pattern}),
                    URLPatternSet({all_url_pattern})));
  modifier.GrantHostPermission(kGoogleCom);

  // Even though <all_urls> has been granted, it was granted as a runtime host
  // pattern, so the extension is still is considered to have withheld host
  // permissions.
  PermissionsManager* permissions_manager =
      PermissionsManager::Get(browser()->profile());
  EXPECT_TRUE(permissions_manager->HasWithheldHostPermissions(*extension));
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, kGoogleCom));
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, kChromiumCom));

  // Changing to specific sites should now remove the broad pattern, leaving
  // only the google match pattern.
  RunUpdateHostAccess(*extension, "ON_SPECIFIC_SITES");
  EXPECT_TRUE(permissions_manager->HasWithheldHostPermissions(*extension));
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, kGoogleCom));
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension, kChromiumCom));
}

TEST_F(DeveloperPrivateApiUnitTest,
       UpdateHostAccess_GrantScopeGreaterThanRequestedScope) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test").AddHostPermission("http://*/*").Build();
  service()->AddExtension(extension.get());
  ScriptingPermissionsModifier modifier(profile(), extension.get());
  modifier.SetWithholdHostPermissions(true);

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());
  EXPECT_EQ(PermissionSet(),
            extension->permissions_data()->active_permissions());
  EXPECT_EQ(PermissionSet(),
            *extension_prefs->GetRuntimeGrantedPermissions(extension->id()));

  {
    auto function =
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
    auto function = base::MakeRefCounted<
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
      ExtensionBuilder("test").AddHostPermission("http://google.com/*").Build();
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
  const char* kEventName =
      api::developer_private::OnItemStateChanged::kEventName;
  event_router->AddEventListener(kEventName, render_process_host(),
                                 listener_id);

  TestEventRouterObserver test_observer(event_router);
  EXPECT_FALSE(WasItemChangedEventDispatched(
      test_observer, extension->id(),
      api::developer_private::EventType::kPermissionsChanged));

  URLPatternSet hosts({URLPattern(Extension::kValidHostPermissionSchemes,
                                  "https://example.com/*")});
  PermissionSet permissions(APIPermissionSet(), ManifestPermissionSet(),
                            hosts.Clone(), hosts.Clone());
  permissions_test_util::GrantRuntimePermissionsAndWaitForCompletion(
      profile(), *extension, permissions);

  // The event router fetches icons from a blocking thread when sending the
  // update event; allow it to finish before verifying the event was dispatched.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(WasItemChangedEventDispatched(
      test_observer, extension->id(),
      api::developer_private::EventType::kPermissionsChanged));

  test_observer.ClearEvents();

  permissions_test_util::RevokeRuntimePermissionsAndWaitForCompletion(
      profile(), *extension, permissions);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(WasItemChangedEventDispatched(
      test_observer, extension->id(),
      api::developer_private::EventType::kPermissionsChanged));
}

TEST_F(DeveloperPrivateApiUnitTest, ExtensionUpdatedEventOnPermissionsChange) {
  // We need to call DeveloperPrivateAPI::Get() in order to instantiate the
  // keyed service, since it's not created by default in unit tests.
  DeveloperPrivateAPI::Get(profile());
  const ExtensionId listener_id = crx_file::id_util::GenerateId("listener");
  EventRouter* event_router = EventRouter::Get(profile());

  // The DeveloperPrivateEventRouter will only dispatch events if there's at
  // least one listener to dispatch to. Create one.
  const char* kEventName =
      api::developer_private::OnItemStateChanged::kEventName;
  event_router->AddEventListener(kEventName, render_process_host(),
                                 listener_id);

  scoped_refptr<const Extension> dummy_extension =
      ExtensionBuilder("dummy")
          .SetManifestKey("optional_permissions",
                          base::Value::List().Append("tabs"))
          .Build();

  TestEventRouterObserver test_observer(event_router);
  EXPECT_FALSE(WasItemChangedEventDispatched(
      test_observer, dummy_extension->id(),
      api::developer_private::EventType::kPermissionsChanged));

  APIPermissionSet apis;
  apis.insert(extensions::mojom::APIPermissionID::kTab);
  PermissionSet permissions(std::move(apis), ManifestPermissionSet(),
                            URLPatternSet(), URLPatternSet());
  permissions_test_util::GrantOptionalPermissionsAndWaitForCompletion(
      profile(), *dummy_extension, permissions);

  // The event router fetches icons from a blocking thread when sending the
  // update event; allow it to finish before verifying the event was dispatched.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(WasItemChangedEventDispatched(
      test_observer, dummy_extension->id(),
      api::developer_private::EventType::kPermissionsChanged));

  test_observer.ClearEvents();

  permissions_test_util::RevokeOptionalPermissionsAndWaitForCompletion(
      profile(), *dummy_extension, permissions,
      PermissionsUpdater::REMOVE_HARD);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(WasItemChangedEventDispatched(
      test_observer, dummy_extension->id(),
      api::developer_private::EventType::kPermissionsChanged));
}

class DeveloperPrivateApiZipFileUnitTest
    : public DeveloperPrivateApiUnitTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    DeveloperPrivateApiUnitTest::SetUp();
    expected_extension_install_directory_ =
        service()->unpacked_install_directory();
  }

 protected:
  base::FilePath expected_extension_install_directory_;
};

TEST_F(DeveloperPrivateApiZipFileUnitTest, InstallDroppedFileZip) {
  base::FilePath zip_path = data_dir().AppendASCII("simple_empty.zip");
  base::AutoReset<bool> disable_ui =
      ExtensionInstallUI::disable_ui_for_tests(true);
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  DeveloperPrivateAPI::Get(profile())->SetDraggedPath(web_contents.get(),
                                                      zip_path);

  auto function =
      base::MakeRefCounted<api::DeveloperPrivateInstallDroppedFileFunction>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());

  TestExtensionRegistryObserver observer(registry());
  ASSERT_TRUE(api_test_utils::RunFunction(function.get(), "[]", profile()))
      << function->GetError();
  scoped_refptr<const Extension> extension =
      observer.WaitForExtensionInstalled();
  ASSERT_TRUE(extension);
  EXPECT_EQ("Simple Empty Extension", extension->name());

  // Expect extension install directory to be immediate subdir of expected
  // unpacked install directory. E.g. /a/b/c/d == /a/b/c + /d.
  //
  // Make sure we're comparing absolute paths to avoid failures like
  // https://crbug.com/1453671 on macOS 14.
  base::FilePath absolute_extension_path =
      base::MakeAbsoluteFilePath(extension->path());
  base::FilePath absolute_expected_extension_install_directory =
      base::MakeAbsoluteFilePath(expected_extension_install_directory_.Append(
          extension->path().BaseName()));
  EXPECT_EQ(absolute_extension_path,
            absolute_expected_extension_install_directory);

  // Expect extension install directory to exist and be named with the right
  // prefix.
  EXPECT_TRUE(base::PathExists(extension->path()));
  EXPECT_TRUE(
      extension->path().BaseName().AsUTF8Unsafe().starts_with("simple_empty"));
}

// Test developerPrivate.getUserSiteSettings.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateGetUserSiteSettings) {
  PermissionsManager* manager = PermissionsManager::Get(browser_context());
  const url::Origin restricted_url =
      url::Origin::Create(GURL("http://example.com"));

  manager->AddUserRestrictedSite(restricted_url);

  auto function =
      base::MakeRefCounted<api::DeveloperPrivateGetUserSiteSettingsFunction>();

  std::optional<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), /*args=*/"[]", profile());
  ASSERT_TRUE(result.has_value());
  std::optional<api::developer_private::UserSiteSettings> settings =
      api::developer_private::UserSiteSettings::FromValue(result.value());

  ASSERT_TRUE(settings);
  EXPECT_THAT(settings->permitted_sites, testing::IsEmpty());
  EXPECT_THAT(settings->restricted_sites,
              testing::UnorderedElementsAre("http://example.com"));
}

// Test developerPrivate.addUserSpecifiedSite and removeUserSpecifiedSite for
// restricted sites.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateModifyUserSiteSettings) {
  static constexpr char kExample[] = "http://example.com";
  static constexpr char kChromium[] = "http://chromium.org";

  const url::Origin example_url = url::Origin::Create(GURL(kExample));
  const url::Origin chromium_url = url::Origin::Create(GURL(kChromium));

  // Add restricted sites, and check that these sites are stored in the manager.
  EXPECT_NO_FATAL_FAILURE(AddUserSpecifiedSites(
      profile(), base::StringPrintf(R"(["%s","%s"])", kExample, kChromium),
      /*restricted=*/true));

  PermissionsManager* manager = PermissionsManager::Get(browser_context());
  EXPECT_THAT(manager->GetUserPermissionsSettings().permitted_sites,
              testing::IsEmpty());
  EXPECT_THAT(manager->GetUserPermissionsSettings().restricted_sites,
              testing::UnorderedElementsAre(example_url, chromium_url));

  // Remove restricted site, and check that the site was removed in the manager.
  EXPECT_NO_FATAL_FAILURE(RemoveUserSpecifiedSites(
      profile(), base::StringPrintf(R"(["%s"])", kExample),
      /*restricted=*/true));

  EXPECT_THAT(manager->GetUserPermissionsSettings().permitted_sites,
              testing::IsEmpty());
  EXPECT_THAT(manager->GetUserPermissionsSettings().restricted_sites,
              testing::UnorderedElementsAre(chromium_url));
}

// Test that the OnUserSiteSettingsChanged event is fired whenever the user
// defined site settings update.
TEST_F(DeveloperPrivateApiUnitTest, OnUserSiteSettingsChanged) {
  static constexpr char kExample[] = "http://example.com";

  // We need to call DeveloperPrivateAPI::Get() in order to instantiate the
  // keyed service, since it's not created by default in unit tests.
  DeveloperPrivateAPI::Get(profile());
  EventRouter* event_router = EventRouter::Get(profile());

  // The DeveloperPrivateEventRouter will only dispatch events if there's at
  // least one listener to dispatch to. Create one.
  const char* kEventName =
      api::developer_private::OnUserSiteSettingsChanged::kEventName;
  event_router->AddEventListener(kEventName, render_process_host(),
                                 crx_file::id_util::GenerateId("listener"));

  TestEventRouterObserver test_observer(event_router);

  api::developer_private::UserSiteSettings settings;
  EXPECT_FALSE(
      WasUserSiteSettingsChangedEventDispatched(test_observer, &settings));

  // Add a restricted site, and check the event that it's
  // only contained in the restricted list.
  const std::string kExampleArg = base::StringPrintf(R"(["%s"])", kExample);
  EXPECT_NO_FATAL_FAILURE(
      AddUserSpecifiedSites(profile(), kExampleArg, /*restricted=*/true));
  EXPECT_TRUE(
      WasUserSiteSettingsChangedEventDispatched(test_observer, &settings));
  EXPECT_THAT(settings.permitted_sites, testing::IsEmpty());
  EXPECT_THAT(settings.restricted_sites,
              testing::UnorderedElementsAre(kExample));

  // Remove the site, and check the event that both lists are empty.
  EXPECT_NO_FATAL_FAILURE(
      RemoveUserSpecifiedSites(profile(), kExampleArg, /*restricted=*/true));
  EXPECT_TRUE(
      WasUserSiteSettingsChangedEventDispatched(test_observer, &settings));
  EXPECT_THAT(settings.permitted_sites, testing::IsEmpty());
  EXPECT_THAT(settings.restricted_sites, testing::IsEmpty());
}

class DeveloperPrivateApiWithPermittedSitesUnitTest
    : public DeveloperPrivateApiUnitTest {
 public:
  DeveloperPrivateApiWithPermittedSitesUnitTest();
  DeveloperPrivateApiWithPermittedSitesUnitTest(
      const DeveloperPrivateApiWithPermittedSitesUnitTest&) = delete;
  const DeveloperPrivateApiWithPermittedSitesUnitTest& operator=(
      const DeveloperPrivateApiWithPermittedSitesUnitTest&) = delete;
  ~DeveloperPrivateApiWithPermittedSitesUnitTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

DeveloperPrivateApiWithPermittedSitesUnitTest::
    DeveloperPrivateApiWithPermittedSitesUnitTest() {
  feature_list_.InitAndEnableFeature(
      extensions_features::kExtensionsMenuAccessControlWithPermittedSites);
}

// Test developerPrivate.getUserSiteSettings.
TEST_F(DeveloperPrivateApiWithPermittedSitesUnitTest,
       DeveloperPrivateGetUserSiteSettings) {
  PermissionsManager* manager = PermissionsManager::Get(browser_context());
  const url::Origin permitted_url =
      url::Origin::Create(GURL("http://a.example.com"));
  const url::Origin restricted_url =
      url::Origin::Create(GURL("http://b.example.com"));

  manager->AddUserPermittedSite(permitted_url);
  manager->AddUserRestrictedSite(restricted_url);

  auto function =
      base::MakeRefCounted<api::DeveloperPrivateGetUserSiteSettingsFunction>();

  base::Value::List args;
  EXPECT_TRUE(RunFunction(function, args)) << function->GetError();
  ASSERT_TRUE(function->GetResultListForTest());
  ASSERT_EQ(1u, function->GetResultListForTest()->size());
  const base::Value& response_value = (*function->GetResultListForTest())[0];
  std::optional<api::developer_private::UserSiteSettings> settings =
      api::developer_private::UserSiteSettings::FromValue(response_value);

  ASSERT_TRUE(settings);
  EXPECT_THAT(settings->permitted_sites,
              testing::UnorderedElementsAre("http://a.example.com"));
  EXPECT_THAT(settings->restricted_sites,
              testing::UnorderedElementsAre("http://b.example.com"));
}

// Test developerPrivate.addUserSpecifiedSite and removeUserSpecifiedSite.
TEST_F(DeveloperPrivateApiWithPermittedSitesUnitTest,
       DeveloperPrivateModifyUserSiteSettings) {
  static constexpr char kExample[] = "http://example.com";
  static constexpr char kChromium[] = "http://chromium.org";
  static constexpr char kGoogle[] = "http://google.com";

  const url::Origin example_url = url::Origin::Create(GURL(kExample));
  const url::Origin chromium_url = url::Origin::Create(GURL(kChromium));
  const url::Origin google_url = url::Origin::Create(GURL(kGoogle));

  auto get_hosts_arg = [](const char* host) {
    return base::StringPrintf(R"(["%s"])", host);
  };

  // First, add some permitted and restricted sites, and check that these sites
  // are stored in the manager.
  EXPECT_NO_FATAL_FAILURE(AddUserSpecifiedSites(
      profile(), base::StringPrintf(R"(["%s","%s"])", kExample, kChromium),
      /*restricted=*/false));
  EXPECT_NO_FATAL_FAILURE(AddUserSpecifiedSites(
      profile(), get_hosts_arg(kGoogle), /*restricted=*/true));

  PermissionsManager* manager = PermissionsManager::Get(browser_context());
  EXPECT_THAT(manager->GetUserPermissionsSettings().permitted_sites,
              testing::UnorderedElementsAre(example_url, chromium_url));
  EXPECT_THAT(manager->GetUserPermissionsSettings().restricted_sites,
              testing::UnorderedElementsAre(google_url));

  // Attempting to add a restricted site should remove it as a permitted site.
  EXPECT_NO_FATAL_FAILURE(AddUserSpecifiedSites(
      profile(), get_hosts_arg(kChromium), /*restricted=*/true));
  EXPECT_NO_FATAL_FAILURE(RemoveUserSpecifiedSites(
      profile(), get_hosts_arg(kExample), /*restricted=*/false));

  EXPECT_TRUE(manager->GetUserPermissionsSettings().permitted_sites.empty());
  EXPECT_THAT(manager->GetUserPermissionsSettings().restricted_sites,
              testing::UnorderedElementsAre(chromium_url, google_url));

  EXPECT_NO_FATAL_FAILURE(RemoveUserSpecifiedSites(
      profile(), base::StringPrintf(R"(["%s","%s"])", kGoogle, kChromium),
      /*restricted=*/true));
  EXPECT_TRUE(manager->GetUserPermissionsSettings().restricted_sites.empty());
}

TEST_F(DeveloperPrivateApiWithPermittedSitesUnitTest,
       DeveloperPrivateGetUserAndExtensionSitesByEtld_UserSites) {
  PermissionsManager* manager = PermissionsManager::Get(browser_context());

  // Add two sites under the eTLD+1 example.com, and one under eTLD+1 google.ca.
  manager->AddUserPermittedSite(
      url::Origin::Create(GURL("http://a.example.com")));
  manager->AddUserRestrictedSite(
      url::Origin::Create(GURL("http://b.example.com")));
  manager->AddUserRestrictedSite(url::Origin::Create(GURL("http://google.ca")));

  auto function = base::MakeRefCounted<
      api::DeveloperPrivateGetUserAndExtensionSitesByEtldFunction>();
  EXPECT_TRUE(RunFunction(function, base::Value::List()))
      << function->GetError();
  const base::Value::List* results = function->GetResultListForTest();
  ASSERT_EQ(1u, results->size());

  EXPECT_THAT((*results)[0], base::test::IsJson(R"([{
    "etldPlusOne": "example.com",
    "numExtensions": 0,
    "sites": [{
      "siteSet": "USER_PERMITTED",
      "numExtensions": 0,
      "site": "a.example.com",
    }, {
      "siteSet": "USER_RESTRICTED",
      "numExtensions": 0,
      "site": "b.example.com",
    }]
  }, {
    "etldPlusOne": "google.ca",
    "numExtensions": 0,
    "sites": [{
      "siteSet": "USER_RESTRICTED",
      "numExtensions": 0,
      "site": "google.ca",
    }]
  }])"));
}

TEST_F(DeveloperPrivateApiWithPermittedSitesUnitTest,
       DeveloperPrivateGetUserAndExtensionSitesByEtld_UserAndExtensionSites) {
  PermissionsManager* manager = PermissionsManager::Get(browser_context());
  manager->AddUserPermittedSite(
      url::Origin::Create(GURL("http://images.google.com")));
  manager->AddUserRestrictedSite(
      url::Origin::Create(GURL("http://www.asdf.com")));

  scoped_refptr<const Extension> extension_1 =
      ExtensionBuilder("test")
          .AddHostPermission("https://*.google.com/")
          .AddHostPermission("http://www.google.com/")
          .AddHostPermission("http://images.google.com/")
          .AddHostPermission("https://example.com/")
          .AddHostPermission("*://localhost/")
          .Build();

  scoped_refptr<const Extension> extension_2 =
      ExtensionBuilder("test_2")
          .AddHostPermission("https://mail.google.com/")
          .AddHostPermission("http://www.google.com/")
          .AddHostPermission("http://www.asdf.com/")
          .AddHostPermission("http://localhost:8080/")
          .Build();
  AddExtensionAndGrantPermissions(profile(), service(), *extension_1);
  AddExtensionAndGrantPermissions(profile(), service(), *extension_2);

  auto function = base::MakeRefCounted<
      api::DeveloperPrivateGetUserAndExtensionSitesByEtldFunction>();
  EXPECT_TRUE(RunFunction(function, base::Value::List()))
      << function->GetError();
  const base::Value::List* results = function->GetResultListForTest();
  ASSERT_EQ(1u, results->size());

  // asdf.com and http://www.asdf.com should not have any extensions counted
  // because they are associated with user specified sites.
  EXPECT_THAT((*results)[0], base::test::IsJson(R"([{
    "etldPlusOne": "asdf.com",
    "numExtensions": 0,
    "sites": [{
      "siteSet": "USER_RESTRICTED",
      "numExtensions": 0,
      "site": "www.asdf.com",
    }]
  }, {
    "etldPlusOne": "example.com",
    "numExtensions": 1,
    "sites": [{
      "siteSet": "EXTENSION_SPECIFIED",
      "numExtensions": 1,
      "site": "example.com",
    }]
  }, {
    "etldPlusOne": "google.com",
    "numExtensions": 2,
    "sites": [{
      "siteSet": "USER_PERMITTED",
      "numExtensions": 0,
      "site": "images.google.com",
    }, {
      "siteSet": "EXTENSION_SPECIFIED",
      "numExtensions": 2,
      "site": "mail.google.com",
    }, {
      "siteSet": "EXTENSION_SPECIFIED",
      "numExtensions": 2,
      "site": "www.google.com",
    }, {
      "siteSet": "EXTENSION_SPECIFIED",
      "numExtensions": 1,
      "site": "*.google.com",
    },]
  }, {
    "etldPlusOne": "localhost",
    "numExtensions": 2,
    "sites": [{
      "siteSet": "EXTENSION_SPECIFIED",
      "numExtensions": 2,
      "site": "localhost",
    }]
  }])"));
}

TEST_F(DeveloperPrivateApiWithPermittedSitesUnitTest,
       DeveloperPrivateGetUserAndExtensionSitesByEtld_EffectiveAllHosts) {
  PermissionsManager* manager = PermissionsManager::Get(browser_context());
  manager->AddUserPermittedSite(
      url::Origin::Create(GURL("http://images.google.ca")));
  manager->AddUserRestrictedSite(url::Origin::Create(GURL("https://yahoo.ca")));

  scoped_refptr<const Extension> extension_1 =
      ExtensionBuilder("specific_hosts")
          .AddHostPermission("https://*.google.ca/")
          .AddHostPermission("http://www.example.com/")
          .Build();

  scoped_refptr<const Extension> extension_2 =
      ExtensionBuilder("all_.com").AddHostPermission("*://*.com/*").Build();

  scoped_refptr<const Extension> extension_3 =
      ExtensionBuilder("all_urls").AddHostPermission("<all_urls>").Build();
  AddExtensionAndGrantPermissions(profile(), service(), *extension_1);
  AddExtensionAndGrantPermissions(profile(), service(), *extension_2);
  AddExtensionAndGrantPermissions(profile(), service(), *extension_3);

  auto function = base::MakeRefCounted<
      api::DeveloperPrivateGetUserAndExtensionSitesByEtldFunction>();
  EXPECT_TRUE(RunFunction(function, base::Value::List()))
      << function->GetError();
  const base::Value::List* results = function->GetResultListForTest();
  ASSERT_EQ(1u, results->size());

  // `extension_2` should not be counted for https://*.google.ca/* as it
  // cannot run on .ca sites.
  EXPECT_THAT((*results)[0], base::test::IsJson(R"([{
    "etldPlusOne": "example.com",
    "numExtensions": 3,
    "sites": [{
      "siteSet": "EXTENSION_SPECIFIED",
      "numExtensions": 3,
      "site": "www.example.com",
    }, {
      "siteSet": "EXTENSION_SPECIFIED",
      "numExtensions": 2,
      "site": "*.example.com",
    }]
  }, {
    "etldPlusOne": "google.ca",
    "numExtensions": 2,
    "sites": [{
      "siteSet": "USER_PERMITTED",
      "numExtensions": 0,
      "site": "images.google.ca",
    }, {
      "siteSet": "EXTENSION_SPECIFIED",
      "numExtensions": 2,
      "site": "*.google.ca",
    }]
  }, {
    "etldPlusOne": "yahoo.ca",
    "numExtensions": 1,
    "sites": [{
      "siteSet": "USER_RESTRICTED",
      "numExtensions": 0,
      "site": "yahoo.ca",
    }, {
      "siteSet": "EXTENSION_SPECIFIED",
      "numExtensions": 1,
      "site": "*.yahoo.ca",
    }]
  }])"));
}

TEST_F(DeveloperPrivateApiUnitTest,
       DeveloperPrivateGetUserAndExtensionSitesByEtld_RuntimeGrantedHosts) {
  scoped_refptr<const Extension> extension_1 =
      ExtensionBuilder("runtime_hosts").AddHostPermission("<all_urls>").Build();
  AddExtensionAndGrantPermissions(profile(), service(), *extension_1);

  auto get_user_and_extension_sites = [this](const std::string& expected_json) {
    auto function = base::MakeRefCounted<
        api::DeveloperPrivateGetUserAndExtensionSitesByEtldFunction>();
    EXPECT_TRUE(RunFunction(function, base::Value::List()))
        << function->GetError();
    const base::Value::List* results = function->GetResultListForTest();
    ASSERT_EQ(1u, results->size());
    EXPECT_THAT((*results)[0], base::test::IsJson(expected_json));
  };

  get_user_and_extension_sites(R"([])");

  EXPECT_FALSE(PermissionsManager::Get(browser()->profile())
                   ->HasWithheldHostPermissions(*extension_1));

  ScriptingPermissionsModifier modifier(profile(), extension_1.get());
  modifier.SetWithholdHostPermissions(true);

  get_user_and_extension_sites(R"([])");

  const std::string kExampleCom = "https://example.com/*";
  RunAddHostPermission(profile(), *extension_1, kExampleCom,
                       /*should_succeed=*/true, nullptr);

  get_user_and_extension_sites(R"([{
    "etldPlusOne": "example.com",
    "numExtensions": 1,
    "sites": [{
      "siteSet": "EXTENSION_SPECIFIED",
      "numExtensions": 1,
      "site": "example.com",
    }]
  }])");

  scoped_refptr<const Extension> extension_2 =
      ExtensionBuilder("test").AddHostPermission(kExampleCom).Build();
  AddExtensionAndGrantPermissions(profile(), service(), *extension_2);

  get_user_and_extension_sites(R"([{
    "etldPlusOne": "example.com",
    "numExtensions": 2,
    "sites": [{
      "siteSet": "EXTENSION_SPECIFIED",
      "numExtensions": 2,
      "site": "example.com",
    }]
  }])");

  RunUpdateHostAccess(*extension_1, "ON_ALL_SITES");
  get_user_and_extension_sites(R"([{
    "etldPlusOne": "example.com",
    "numExtensions": 2,
    "sites": [{
      "siteSet": "EXTENSION_SPECIFIED",
      "numExtensions": 2,
      "site": "example.com",
    }, {
      "siteSet": "EXTENSION_SPECIFIED",
      "numExtensions": 1,
      "site": "*.example.com",
    }]
  }])");
}

// Test that host permissions from policy installed extensions are included in
// `getUserAndExtensionSitesByEtld` calls.
TEST_F(
    DeveloperPrivateApiUnitTest,
    DeveloperPrivateGetUserAndExtensionSitesByEtld_PolicyControlledExtensions) {
  ExtensionId extension_id(kGoogleOnlyCrx);

  // Set up a mock provider with a policy extension.
  std::unique_ptr<MockExternalProvider> mock_provider =
      std::make_unique<MockExternalProvider>(
          service(), mojom::ManifestLocation::kExternalPolicyDownload);
  MockExternalProvider* mock_provider_ptr = mock_provider.get();
  AddMockExternalProvider(std::move(mock_provider));

  // google_only.crx contains only a manifest.json file that requests
  // *://www.google.com/* as a permission.
  mock_provider_ptr->UpdateOrAddExtension(
      extension_id, "1", data_dir().AppendASCII("google_only.crx"));
  // Reloading extensions should find our externally registered extension
  // and install it.
  {
    TestExtensionRegistryObserver observer(registry());
    service()->CheckForExternalUpdates();
    EXPECT_EQ(extension_id, observer.WaitForExtensionLoaded()->id());
  }

  auto function = base::MakeRefCounted<
      api::DeveloperPrivateGetUserAndExtensionSitesByEtldFunction>();
  EXPECT_TRUE(RunFunction(function, base::Value::List()))
      << function->GetError();
  const base::Value::List* results = function->GetResultListForTest();
  ASSERT_EQ(1u, results->size());

  EXPECT_THAT((*results)[0], base::test::IsJson(R"([{
    "etldPlusOne": "google.com",
    "numExtensions": 1,
    "sites": [{
      "siteSet": "EXTENSION_SPECIFIED",
      "numExtensions": 1,
      "site": "www.google.com",
    }]
  }])"));
}

TEST_F(DeveloperPrivateApiUnitTest,
       DeveloperPrivateGetMatchingExtensionsForSite) {
  namespace developer = api::developer_private;

  scoped_refptr<const Extension> extension_1 =
      ExtensionBuilder("test")
          .AddHostPermission("*://mail.google.com/")
          .Build();

  scoped_refptr<const Extension> extension_2 =
      ExtensionBuilder("test_2")
          .AddHostPermission("*://images.google.com/")
          .Build();
  AddExtensionAndGrantPermissions(profile(), service(), *extension_1);
  AddExtensionAndGrantPermissions(profile(), service(), *extension_2);

  std::vector<developer::MatchingExtensionInfo> infos;
  GetMatchingExtensionsForSite(profile(), "http://none.com/", &infos);
  EXPECT_TRUE(infos.empty());

  GetMatchingExtensionsForSite(profile(), "http://images.google.com/", &infos);

  // "http://images.google.com/" should only match with `extension_2`.
  EXPECT_THAT(infos,
              testing::UnorderedElementsAre(MatchMatchingExtensionInfo(
                  extension_2->id(), developer::HostAccess::kOnSpecificSites,
                  /*can_request_all_sites=*/false)));

  service()->DisableExtension(extension_2->id(),
                              disable_reason::DISABLE_USER_ACTION);
  GetMatchingExtensionsForSite(profile(), "*://*.google.com/", &infos);

  // "*://*.google.com/" should match with `extension_1` but not `extension_2`
  // since it is disabled.
  EXPECT_THAT(infos,
              testing::UnorderedElementsAre(MatchMatchingExtensionInfo(
                  extension_1->id(), developer::HostAccess::kOnSpecificSites,
                  /*can_request_all_sites=*/false)));
}

// Test that the host access returned by GetMatchingExtensionsForSite reflects
// whether the extension has access to the queried site, or has withheld sites
// in general.
TEST_F(DeveloperPrivateApiUnitTest,
       DeveloperPrivateGetMatchingExtensionsForSite_RuntimeGrantedHostAccess) {
  namespace developer = api::developer_private;

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test").AddHostPermission("<all_urls>").Build();
  AddExtensionAndGrantPermissions(profile(), service(), *extension);

  std::vector<developer::MatchingExtensionInfo> infos;
  GetMatchingExtensionsForSite(profile(), "http://example.com/", &infos);

  EXPECT_THAT(infos, testing::UnorderedElementsAre(MatchMatchingExtensionInfo(
                         extension->id(), developer::HostAccess::kOnAllSites,
                         /*can_request_all_sites=*/true)));
  EXPECT_FALSE(PermissionsManager::Get(browser()->profile())
                   ->HasWithheldHostPermissions(*extension));

  ScriptingPermissionsModifier modifier(profile(), extension.get());
  modifier.SetWithholdHostPermissions(true);

  GetMatchingExtensionsForSite(profile(), "http://example.com/", &infos);
  EXPECT_THAT(infos, testing::UnorderedElementsAre(MatchMatchingExtensionInfo(
                         extension->id(), developer::HostAccess::kOnClick,
                         /*can_request_all_sites=*/true)));

  RunAddHostPermission(profile(), *extension, "*://*.google.com/*",
                       /*should_succeed=*/true, nullptr);

  GetMatchingExtensionsForSite(profile(), "http://google.com/", &infos);
  EXPECT_THAT(infos,
              testing::UnorderedElementsAre(MatchMatchingExtensionInfo(
                  extension->id(), developer::HostAccess::kOnSpecificSites,
                  /*can_request_all_sites=*/true)));

  GetMatchingExtensionsForSite(profile(), "http://example.com/", &infos);
  EXPECT_THAT(infos, testing::UnorderedElementsAre(MatchMatchingExtensionInfo(
                         extension->id(), developer::HostAccess::kOnClick,
                         /*can_request_all_sites=*/true)));
}

// Tests the UpdateSiteAccess function when called on an extension with no
// withheld host permissions.
TEST_F(DeveloperPrivateApiUnitTest,
       DeveloperPrivateUpdateSiteAccess_NoWithheldHostPermissions) {
  namespace developer = api::developer_private;

  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test")
          .AddHostPermission("http://a.example.com/*")
          .AddHostPermission("*://b.example.com/*")
          .AddHostPermission("http://google.com/*")
          .Build();
  AddExtensionAndGrantPermissions(profile(), service(), *extension);

  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  EXPECT_FALSE(permissions_manager->HasWithheldHostPermissions(*extension));

  // Change state from ON_ALL_SITES to ON_CLICK.
  std::vector<developer::ExtensionSiteAccessUpdate> updates;
  updates.push_back(
      CreateSiteAccessUpdate(extension->id(), developer::HostAccess::kOnClick));
  UpdateSiteAccess(profile(), "http://google.com/*", updates);

  // Check that all host permissions are withheld when the site access is
  // changed to ON_CLICK if there are no withheld host permissions.
  EXPECT_TRUE(permissions_manager->HasWithheldHostPermissions(*extension));
  EXPECT_EQ(PermissionSet(),
            *extension_prefs->GetRuntimeGrantedPermissions(extension->id()));

  // Change state from ON_CLICK to ON_ALL_SITES.
  updates.clear();
  updates.push_back(CreateSiteAccessUpdate(extension->id(),
                                           developer::HostAccess::kOnAllSites));
  UpdateSiteAccess(profile(), "http://google.com/*", updates);

  EXPECT_FALSE(permissions_manager->HasWithheldHostPermissions(*extension));

  // Change state from ON_ALL_SITES to ON_SPECIFIC_SITES.
  updates.clear();
  updates.push_back(CreateSiteAccessUpdate(
      extension->id(), developer::HostAccess::kOnSpecificSites));
  UpdateSiteAccess(profile(), "*://*.example.com/*", updates);

  // Check that the pattern is added as-is to the extension's runtime granted
  // permissions when the site access is changed to ON_SPECIFIC_SITES if there
  // are no withheld host permissions.
  URLPattern example_pattern(Extension::kValidHostPermissionSchemes,
                             "*://*.example.com/*");
  EXPECT_EQ(URLPatternSet({example_pattern}),
            (*extension_prefs->GetRuntimeGrantedPermissions(extension->id()))
                .effective_hosts());

  // Check that the extension's actual active host permissions is an
  // intersection of their manifest and runtime granted hosts.
  URLPattern a_example_pattern(Extension::kValidHostPermissionSchemes,
                               "http://a.example.com/*");
  URLPattern b_example_pattern(Extension::kValidHostPermissionSchemes,
                               "*://b.example.com/*");
  EXPECT_EQ(
      URLPatternSet({a_example_pattern, b_example_pattern}),
      extension->permissions_data()->active_permissions().effective_hosts());
}

// Tests the UpdateSiteAccess function when called on an extension with withheld
// host permissions. In particular, test that if the site access is set to
// ON_CLICK, all host permissions that match the specified site will be revoked.
TEST_F(DeveloperPrivateApiUnitTest,
       DeveloperPrivateUpdateSiteAccess_WitheldHostPermissions) {
  namespace developer = api::developer_private;

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test")
          .AddHostPermission("*://*.example.com/*")
          .AddHostPermission("*://*.google.com/*")
          .Build();
  AddExtensionAndGrantPermissions(profile(), service(), *extension);

  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  EXPECT_FALSE(permissions_manager->HasWithheldHostPermissions(*extension));

  // Change state from ON_ALL_SITES to ON_SPECIFIC_SITES.
  std::vector<developer::ExtensionSiteAccessUpdate> updates;
  updates.push_back(CreateSiteAccessUpdate(
      extension->id(), developer::HostAccess::kOnSpecificSites));
  UpdateSiteAccess(profile(), "http://google.com/*", updates);
  UpdateSiteAccess(profile(), "*://mail.google.com/*", updates);
  UpdateSiteAccess(profile(), "https://maps.google.com/*", updates);
  UpdateSiteAccess(profile(), "*://example.com/*", updates);

  // Confirm that all four sites have been added to runtime granted host
  // permissions.
  const GURL kGoogleCom("http://google.com");
  const GURL kMailGoogleCom("https://mail.google.com/");
  const GURL kMapsGoogleCom("https://maps.google.com/");
  const GURL kExampleCom("http://example.com/");
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, kGoogleCom));
  EXPECT_TRUE(permissions_manager->HasGrantedHostPermission(*extension,
                                                            kMailGoogleCom));
  EXPECT_TRUE(permissions_manager->HasGrantedHostPermission(*extension,
                                                            kMapsGoogleCom));
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, kExampleCom));

  // Change state from ON_SPECIFIC_SITES to ON_CLICK. This will revoke
  // "http://google.com/*", "https://maps.google.com/*", and
  // "*://mail.google.com/*" as they match the pattern "http://*.google.com/*"
  // that is being removed.
  updates.clear();
  updates.push_back(
      CreateSiteAccessUpdate(extension->id(), developer::HostAccess::kOnClick));
  UpdateSiteAccess(profile(), "http://*.google.com/*", updates);

  // The sites `kGoogleCom` and `kMailGoogleCom` match previously granted
  // patterns that were revoked when they matched "http://*.google.com/*" that
  // was called in UpdateSiteAccess. As such, they should no longer be granted.
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension, kGoogleCom));
  EXPECT_FALSE(permissions_manager->HasGrantedHostPermission(*extension,
                                                             kMailGoogleCom));
  EXPECT_TRUE(permissions_manager->HasGrantedHostPermission(*extension,
                                                            kMapsGoogleCom));
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, kExampleCom));

  // Change state from ON_CLICK to ON_SPECIFIC_SITES.
  updates.clear();
  updates.push_back(CreateSiteAccessUpdate(
      extension->id(), developer::HostAccess::kOnSpecificSites));
  UpdateSiteAccess(profile(), "*://mail.google.com/*", updates);
  // `kMailGoogleCom` matches the pattern "*://mail.google.com/*" that is being
  // added, so it should be granted again.
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension, kGoogleCom));
  EXPECT_TRUE(permissions_manager->HasGrantedHostPermission(*extension,
                                                            kMailGoogleCom));
  EXPECT_TRUE(permissions_manager->HasGrantedHostPermission(*extension,
                                                            kMapsGoogleCom));
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension, kExampleCom));
}

// Test that the UpdateSiteAccess function can be applied to multiple
// extensions.
TEST_F(DeveloperPrivateApiUnitTest,
       DeveloperPrivateUpdateSiteAccess_MultipleExtensions) {
  namespace developer = api::developer_private;

  scoped_refptr<const Extension> extension_1 =
      ExtensionBuilder("test_1").AddHostPermission("<all_urls>").Build();
  scoped_refptr<const Extension> extension_2 =
      ExtensionBuilder("test_2").AddHostPermission("<all_urls>").Build();
  AddExtensionAndGrantPermissions(profile(), service(), *extension_1);
  AddExtensionAndGrantPermissions(profile(), service(), *extension_2);

  PermissionsManager* permissions_manager = PermissionsManager::Get(profile());
  EXPECT_FALSE(permissions_manager->HasWithheldHostPermissions(*extension_1));
  EXPECT_FALSE(permissions_manager->HasWithheldHostPermissions(*extension_2));

  std::vector<developer::ExtensionSiteAccessUpdate> updates;
  updates.push_back(CreateSiteAccessUpdate(
      extension_1->id(), developer::HostAccess::kOnSpecificSites));
  updates.push_back(CreateSiteAccessUpdate(extension_2->id(),
                                           developer::HostAccess::kOnClick));
  UpdateSiteAccess(profile(), "http://google.com/*", updates);

  // Confirm that `extension_1` can still access `kGoogleCom` but `extension_2`
  // cannot.
  const GURL kGoogleCom("http://google.com");
  EXPECT_TRUE(
      permissions_manager->HasGrantedHostPermission(*extension_1, kGoogleCom));
  EXPECT_FALSE(
      permissions_manager->HasGrantedHostPermission(*extension_2, kGoogleCom));
}

// Test uninstalling multiple extensions.
TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateRemoveMultipleExtensions) {
  scoped_refptr<const Extension> extension_1 =
      ExtensionBuilder("test_1").Build();
  scoped_refptr<const Extension> extension_2 =
      ExtensionBuilder("test_2").Build();
  service()->AddExtension(extension_1.get());
  service()->AddExtension(extension_2.get());
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_1->id()));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_2->id()));

  std::string args = base::StrCat(
      {"[[\"", extension_1->id(), "\", \"", extension_2->id(), "\"]]"});

  auto function = base::MakeRefCounted<
      api::DeveloperPrivateRemoveMultipleExtensionsFunction>();

  // Accept the multiple extension uninstallation bubble by default in unit
  // tests.
  function->accept_bubble_for_testing(true);

  // Run the private api to remove the installed extensions.
  api_test_utils::RunFunction(function.get(), args, profile());

  EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_1->id()));
  EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_2->id()));
  EXPECT_EQ(registry()->enabled_extensions().size(), 0u);
}

// Test cancelling uninstall multiple extensions dialog.
TEST_F(DeveloperPrivateApiUnitTest,
       DeveloperPrivateCancelRemoveMultipleExtensions) {
  scoped_refptr<const Extension> extension_1 =
      ExtensionBuilder("test_1").Build();
  scoped_refptr<const Extension> extension_2 =
      ExtensionBuilder("test_2").Build();
  service()->AddExtension(extension_1.get());
  service()->AddExtension(extension_2.get());
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_1->id()));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_2->id()));

  std::string args = base::StrCat(
      {"[[\"", extension_1->id(), "\", \"", extension_2->id(), "\"]]"});

  auto function = base::MakeRefCounted<
      api::DeveloperPrivateRemoveMultipleExtensionsFunction>();

  // Cancel the multiple extension uninstallation bubble, the correct error
  // message is shown and extensions are not removed.
  function->accept_bubble_for_testing(false);
  EXPECT_EQ("User cancelled uninstall",
            api_test_utils::RunFunctionAndReturnError(function.get(), args,
                                                      profile()));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_1->id()));
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_2->id()));
  EXPECT_EQ(registry()->enabled_extensions().size(), 2u);
}

TEST_F(DeveloperPrivateApiUnitTest, DeveloperPrivateRemoveComponentExtensions) {
  // Create a component extension and a regular extension, then try to remove
  // them.
  scoped_refptr<const Extension> component_extension =
      ExtensionBuilder("component_extension")
          .SetLocation(mojom::ManifestLocation::kComponent)
          .Build();
  scoped_refptr<const Extension> test_extension =
      ExtensionBuilder("test_extension").Build();
  service()->AddExtension(component_extension.get());
  service()->AddExtension(test_extension.get());

  EXPECT_EQ(registry()->enabled_extensions().size(), 2u);

  // Create a list of extensions with a component extension in it.
  base::Value::List extensions_list;
  extensions_list.reserve(2u);
  extensions_list.Append(component_extension->id());
  extensions_list.Append(test_extension->id());
  std::string args;
  EXPECT_TRUE(base::JSONWriter::Write(extensions_list, &args));
  std::string component_args = base::StringPrintf(R"([%s])", args.c_str());
  auto function = base::MakeRefCounted<
      api::DeveloperPrivateRemoveMultipleExtensionsFunction>();

  // Accept the multiple extension uninstallation bubble by default in unit
  // tests.
  function->accept_bubble_for_testing(true);
  // Verify the error message for uninstalling component and enterprise
  // extensions.
  EXPECT_EQ(
      "Cannot uninstall the enterprise or component extensions in your list.",
      api_test_utils::RunFunctionAndReturnError(function.get(), component_args,
                                                profile()));

  // Because there is a component extension in the list, the uninstallation is
  // canceled. The number of extensions remains the same.
  EXPECT_EQ(registry()->enabled_extensions().size(), 2u);
}

TEST_F(DeveloperPrivateApiUnitTest,
       DeveloperPrivateRemoveEnterpriseExtensions) {
  // Create an enterprise extension and a regular extension, then try to remove
  // them.
  scoped_refptr<const Extension> enterprise_extension =
      ExtensionBuilder("enterprise_extension")
          .SetLocation(mojom::ManifestLocation::kExternalPolicy)
          .Build();
  scoped_refptr<const Extension> test_extension =
      ExtensionBuilder("test_extension").Build();
  service()->AddExtension(enterprise_extension.get());
  service()->AddExtension(test_extension.get());

  EXPECT_EQ(registry()->enabled_extensions().size(), 2u);

  // Create a list of extensions with an enterprise extension in it.
  base::Value::List extensions_list;
  extensions_list.reserve(2u);
  extensions_list.Append(enterprise_extension->id());
  extensions_list.Append(test_extension->id());
  std::string args;
  EXPECT_TRUE(base::JSONWriter::Write(extensions_list, &args));
  std::string enterprise_args = base::StringPrintf(R"([%s])", args.c_str());
  auto function = base::MakeRefCounted<
      api::DeveloperPrivateRemoveMultipleExtensionsFunction>();

  // Accept the multiple extension uninstallation bubble by default in unit
  // tests.
  function->accept_bubble_for_testing(true);
  // Verify the error message for uninstalling component and enterprise
  // extensions.
  EXPECT_EQ(
      "Cannot uninstall the enterprise or component extensions in your list.",
      api_test_utils::RunFunctionAndReturnError(function.get(), enterprise_args,
                                                profile()));

  // Because there is an enterprise extension in the list, the uninstallation is
  // canceled. The number of extensions remains the same.
  EXPECT_EQ(registry()->enabled_extensions().size(), 2u);
}

// Test that an event is dispatched when the list of pinned extension actions
// has changed.
TEST_F(DeveloperPrivateApiUnitTest,
       ExtensionUpdatedEventOnPinnedActionsChange) {
  // We need to call DeveloperPrivateAPI::Get() in order to instantiate the
  // keyed service, since it's not created by default in unit tests.
  DeveloperPrivateAPI::Get(profile());
  EventRouter* event_router = EventRouter::Get(profile());

  // The DeveloperPrivateEventRouter will only dispatch events if there's at
  // least one listener to dispatch to. Create one.
  const char* kEventName =
      api::developer_private::OnItemStateChanged::kEventName;
  event_router->AddEventListener(kEventName, render_process_host(),
                                 crx_file::id_util::GenerateId("listener"));

  TestEventRouterObserver test_observer(event_router);

  scoped_refptr<const Extension> extension = ExtensionBuilder("test").Build();
  service()->AddExtension(extension.get());
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));

  // The event router fetches icons from a blocking thread when sending the
  // update event; allow it to finish before verifying the event was dispatched.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(WasItemChangedEventDispatched(
      test_observer, extension->id(),
      api::developer_private::EventType::kPinnedActionsChanged));

  ToolbarActionsModel* toolbar_actions_model =
      ToolbarActionsModel::Get(profile());

  toolbar_actions_model->SetActionVisibility(
      extension->id(), !toolbar_actions_model->IsActionPinned(extension->id()));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(WasItemChangedEventDispatched(
      test_observer, extension->id(),
      api::developer_private::EventType::kPinnedActionsChanged));
}

class DeveloperPrivateApiAllowlistUnitTest
    : public DeveloperPrivateApiUnitTest {
 public:
  DeveloperPrivateApiAllowlistUnitTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kSafeBrowsingCrxAllowlistShowWarnings);
  }
};

TEST_F(DeveloperPrivateApiAllowlistUnitTest,
       ExtensionUpdatedEventOnAllowlistWarningChange) {
  // We need to call DeveloperPrivateAPI::Get() in order to instantiate the
  // keyed service, since it's not created by default in unit tests.
  DeveloperPrivateAPI::Get(profile());
  const ExtensionId listener_id = crx_file::id_util::GenerateId("listener");
  EventRouter* event_router = EventRouter::Get(profile());

  // The DeveloperPrivateEventRouter will only dispatch events if there's at
  // least one listener to dispatch to. Create one.
  const char* kEventName =
      api::developer_private::OnItemStateChanged::kEventName;
  event_router->AddEventListener(kEventName, render_process_host(),
                                 listener_id);

  scoped_refptr<const Extension> dummy_extension = LoadSimpleExtension();
  base::RunLoop().RunUntilIdle();

  TestEventRouterObserver test_observer(event_router);
  EXPECT_FALSE(WasItemChangedEventDispatched(
      test_observer, dummy_extension->id(),
      api::developer_private::EventType::kPrefsChanged));

  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  base::RunLoop().RunUntilIdle();
  // The warning state should not have changed since the allowlist state is not
  // set yet.
  EXPECT_FALSE(WasItemChangedEventDispatched(
      test_observer, dummy_extension->id(),
      api::developer_private::EventType::kPrefsChanged));

  service()->allowlist()->SetExtensionAllowlistState(dummy_extension->id(),
                                                     ALLOWLIST_NOT_ALLOWLISTED);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(WasItemChangedEventDispatched(
      test_observer, dummy_extension->id(),
      api::developer_private::EventType::kPrefsChanged));

  test_observer.ClearEvents();

  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);

  base::RunLoop().RunUntilIdle();
  // The warning is now hidden because the profile is no longer Enhanced
  // Protection.
  EXPECT_TRUE(WasItemChangedEventDispatched(
      test_observer, dummy_extension->id(),
      api::developer_private::EventType::kPrefsChanged));
}

class DeveloperPrivateApiSupervisedUserUnitTest
    : public DeveloperPrivateApiUnitTest,
      public testing::WithParamInterface<bool> {
 public:
  DeveloperPrivateApiSupervisedUserUnitTest() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
    if (extensions_permissions_for_supervised_users_on_desktop()) {
      feature_list_.InitAndEnableFeature(
          supervised_user::
              kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);

    } else {
      feature_list_.InitAndDisableFeature(
          supervised_user::
              kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
    }
#endif
  }

  DeveloperPrivateApiSupervisedUserUnitTest(
      const DeveloperPrivateApiSupervisedUserUnitTest&) = delete;
  DeveloperPrivateApiSupervisedUserUnitTest& operator=(
      const DeveloperPrivateApiSupervisedUserUnitTest&) = delete;

  ~DeveloperPrivateApiSupervisedUserUnitTest() override = default;

  bool ProfileIsSupervised() const override { return true; }

  bool extensions_permissions_for_supervised_users_on_desktop() const {
    return GetParam();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests trying to call loadUnpacked when the profile shouldn't be allowed to.
TEST_P(DeveloperPrivateApiSupervisedUserUnitTest,
       LoadUnpackedFailsForSupervisedUsers) {
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  base::FilePath path = data_dir().AppendASCII("simple_with_popup");

  if (extensions_permissions_for_supervised_users_on_desktop()) {
    EXPECT_TRUE(supervised_user::AreExtensionsPermissionsEnabled(profile()));
    auto function =
        base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function.get(), "[]", profile());
    EXPECT_THAT(error, testing::HasSubstr("Child account"));
  } else {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
    EXPECT_TRUE(supervised_user::AreExtensionsPermissionsEnabled(profile()));
    auto function =
        base::MakeRefCounted<api::DeveloperPrivateLoadUnpackedFunction>();
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function.get(), "[]", profile());
    EXPECT_THAT(error, testing::HasSubstr("Child account"));
#else
    EXPECT_FALSE(supervised_user::AreExtensionsPermissionsEnabled(profile()));
#endif
  }
}

INSTANTIATE_TEST_SUITE_P(
    ExtensionsPermissionsForSupervisedUsersOnDesktopFeature,
    DeveloperPrivateApiSupervisedUserUnitTest,
    testing::Bool());

// Test suite for cases where the user is in the  MV2 deprecation "warning"
// experiment phase.
class DeveloperPrivateApiWithMV2DeprecationWarningUnitTest
    : public DeveloperPrivateApiUnitTest {
 public:
  DeveloperPrivateApiWithMV2DeprecationWarningUnitTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionManifestV2DeprecationWarning);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test suite for cases where the user is in the  MV2 deprecation "disabled"
// experiment phase.
class DeveloperPrivateApiWithMV2DeprecationDisabledUnitTest
    : public DeveloperPrivateApiUnitTest {
 public:
  DeveloperPrivateApiWithMV2DeprecationDisabledUnitTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionManifestV2Disabled);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(DeveloperPrivateApiWithMV2DeprecationWarningUnitTest,
       TestAcknowledgingAnExtension) {
  // Add an extension that is affected by the MV2 deprecation.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("ext").SetManifestVersion(2).Build();
  service()->AddExtension(extension.get());

  ManifestV2ExperimentManager* experiment_manager =
      ManifestV2ExperimentManager::Get(browser_context());
  EXPECT_TRUE(experiment_manager->IsExtensionAffected(*extension));
  EXPECT_FALSE(experiment_manager->DidUserAcknowledgeNotice(extension->id()));

  base::Value::List args;
  args.Append(extension->id());

  // Dismiss the extension's notice.
  auto dismiss_notice_function = base::MakeRefCounted<
      api::DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction>();
  dismiss_notice_function->set_source_context_type(mojom::ContextType::kWebUi);
  EXPECT_TRUE(RunFunction(dismiss_notice_function, args));

  // Extension's notice should be marked as acknowledged.
  EXPECT_TRUE(experiment_manager->IsExtensionAffected(*extension));
  EXPECT_TRUE(experiment_manager->DidUserAcknowledgeNotice(extension->id()));
}

TEST_F(DeveloperPrivateApiWithMV2DeprecationWarningUnitTest,
       TestAcknowledgingANonAffectedExtension) {
  // Add an extension that is not affected by the MV2 deprecation.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("ext").SetManifestVersion(3).Build();
  service()->AddExtension(extension.get());

  std::string args = base::StringPrintf(R"(["%s"])", extension->id().c_str());
  auto dismiss_notice_function = base::MakeRefCounted<
      api::DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction>();
  dismiss_notice_function->set_source_context_type(mojom::ContextType::kWebUi);

  // Cannot dismiss an extension's notice whe the extension is not affected by
  // the MV2 deprecation.
  std::string error = api_test_utils::RunFunctionAndReturnError(
      dismiss_notice_function, args, browser()->profile());
  EXPECT_EQ(error,
            ErrorUtils::FormatErrorMessage(
                "Extension with ID '*' is not affected by the MV2 deprecation.",
                extension->id()));

  // Extension notice should not be marked as acknowledged.
  ManifestV2ExperimentManager* experiment_manager =
      ManifestV2ExperimentManager::Get(browser_context());
  EXPECT_FALSE(experiment_manager->DidUserAcknowledgeNotice(extension->id()));
}

TEST_F(DeveloperPrivateApiWithMV2DeprecationWarningUnitTest,
       TestAcknowledgingNoticeGlobally) {
  ManifestV2ExperimentManager* experiment_manager =
      ManifestV2ExperimentManager::Get(browser_context());
  EXPECT_FALSE(experiment_manager->DidUserAcknowledgeNoticeGlobally());

  auto update_profile_function = base::MakeRefCounted<
      api::DeveloperPrivateUpdateProfileConfigurationFunction>();
  update_profile_function->set_source_context_type(mojom::ContextType::kWebUi);

  base::Value::List args;
  args.Append(base::Value::Dict().Set("isMv2DeprecationNoticeDismissed", true));
  EXPECT_TRUE(RunFunction(update_profile_function, args));

  EXPECT_TRUE(experiment_manager->DidUserAcknowledgeNoticeGlobally());
}

TEST_F(DeveloperPrivateApiWithMV2DeprecationDisabledUnitTest,
       TestAcknowledgingAnExtension) {
  // Add an extension that is affected by the MV2 deprecation.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("ext").SetManifestVersion(2).Build();
  service()->AddExtension(extension.get());

  ManifestV2ExperimentManager* experiment_manager =
      ManifestV2ExperimentManager::Get(browser_context());
  EXPECT_TRUE(experiment_manager->IsExtensionAffected(*extension));
  EXPECT_FALSE(experiment_manager->DidUserAcknowledgeNotice(extension->id()));

  base::Value::List args;
  args.Append(extension->id());

  // Call the dismiss notice function, and cancel the dismissal.
  auto dismiss_notice_function = base::MakeRefCounted<
      api::DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction>();
  dismiss_notice_function->set_source_context_type(mojom::ContextType::kWebUi);
  dismiss_notice_function->accept_bubble_for_testing(false);
  EXPECT_TRUE(RunFunction(dismiss_notice_function, args));

  // Extension notice should NOT be marked as acknowledged.
  EXPECT_TRUE(experiment_manager->IsExtensionAffected(*extension));
  EXPECT_FALSE(experiment_manager->DidUserAcknowledgeNotice(extension->id()));

  // Call the dismiss notice function, and accept the dismissal.
  dismiss_notice_function = base::MakeRefCounted<
      api::DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction>();
  dismiss_notice_function->set_source_context_type(mojom::ContextType::kWebUi);
  dismiss_notice_function->accept_bubble_for_testing(true);
  EXPECT_TRUE(RunFunction(dismiss_notice_function, args));

  // Extension's notice should be marked as acknowledged.
  EXPECT_TRUE(experiment_manager->IsExtensionAffected(*extension));
  EXPECT_TRUE(experiment_manager->DidUserAcknowledgeNotice(extension->id()));
}

}  // namespace extensions
