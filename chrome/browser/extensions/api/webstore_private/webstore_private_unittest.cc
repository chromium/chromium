// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <vector>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/extensions/api/webstore_private/webstore_private_api.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/supervised_user/core/common/features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {
constexpr char kInvalidId[] = "Invalid id";
constexpr char kExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kFakeJustification[] = "I need it!";
constexpr char kExtensionManifest[] = R"({
  \"name\" : \"Extension\",
  \"manifest_version\": 3,
  \"version\": \"0.1\",
  \"permissions\": [ \"example.com\", \"downloads\"],
  \"optional_permissions\" : [\"audio\"]})";

constexpr char kBlockAllExtensionSettings[] = R"({
  "*": {
    "installation_mode":"blocked",
    "blocked_install_message":"This extension is blocked."
  }
})";
constexpr char kBlockOneExtensionSettings[] = R"({
  "abcdefghijklmnopabcdefghijklmnop": {
    "installation_mode":"blocked"
  }
})";

constexpr char kBlockedManifestTypeExtensionSettings[] = R"({
  "*": {
    "allowed_types": ["theme", "hosted_app"]
  }
})";

constexpr char kBlockedDownloadsPermissionsExtensionSettings[] = R"({
  "*": {
    "blocked_permissions": ["downloads"]
  }
})";

constexpr char kBlockedAudioPermissionsExtensionSettings[] = R"({
  "*": {
    "blocked_permissions": ["audio"]
  }
})";

constexpr char kWebstoreUserCancelledError[] = "User cancelled install";
constexpr char kWebstoreBlockByPolicy[] =
    "Extension installation is blocked by policy";

// Helper test struct used for holding data related to extension requests.
struct ExtensionRequestData {
  explicit ExtensionRequestData(base::Time timestamp)
      : ExtensionRequestData(timestamp, std::string()) {}
  ExtensionRequestData(base::Time timestamp, std::string justification_text)
      : timestamp(timestamp),
        justification_text(std::move(justification_text)) {}
  ~ExtensionRequestData() = default;

  base::Time timestamp;
  std::string justification_text;
};

// Verifies that the extension request pending list in |profile| matches the
// |expected_pending_requests|.
void VerifyPendingList(const std::map<ExtensionId, ExtensionRequestData>&
                           expected_pending_requests,
                       Profile* profile) {
  const base::Value::Dict& actual_pending_requests =
      profile->GetPrefs()->GetDict(prefs::kCloudExtensionRequestIds);
  ASSERT_EQ(expected_pending_requests.size(), actual_pending_requests.size());
  for (const auto& expected_request : expected_pending_requests) {
    const base::Value::Dict* actual_pending_request =
        actual_pending_requests.FindDict(expected_request.first);
    ASSERT_NE(nullptr, actual_pending_request);

    // All extensions in the pending list are expected to have a timestamp.
    EXPECT_EQ(::base::TimeToValue(expected_request.second.timestamp),
              *actual_pending_request->Find(
                  extension_misc::kExtensionRequestTimestamp));

    // Extensions in the pending list may not have justification.
    if (!expected_request.second.justification_text.empty()) {
      EXPECT_EQ(expected_request.second.justification_text,
                *actual_pending_request->FindString(
                    extension_misc::kExtensionWorkflowJustification));
    } else {
      EXPECT_FALSE(actual_pending_request->contains(
          extension_misc::kExtensionWorkflowJustification));
    }
  }
}

void SetExtensionSettings(const std::string& settings_string,
                          TestingProfile* profile) {
  std::optional<base::Value> settings = base::JSONReader::Read(settings_string);
  ASSERT_TRUE(settings.has_value());
  profile->GetTestingPrefService()->SetManagedPref(
      pref_names::kExtensionManagement,
      base::Value::ToUniquePtrValue(std::move(*settings)));
}

std::unique_ptr<KeyedService> BuildManagementApi(
    content::BrowserContext* context) {
  return std::make_unique<ManagementAPI>(context);
}

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* profile) {
  return std::make_unique<extensions::EventRouter>(
      profile, ExtensionPrefs::Get(profile));
}

}  // namespace

using WebstorePrivateApiUnittest = ExtensionApiUnittest;

TEST_F(WebstorePrivateApiUnittest, GetFullChromeVersion) {
  auto function =
      base::MakeRefCounted<WebstorePrivateGetFullChromeVersionFunction>();
  std::optional<base::Value> response =
      api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), /*args*/ "[]", browser()->profile());
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->is_dict());

  std::string version = std::string(version_info::GetVersionNumber());
  EXPECT_EQ(version, *response->GetDict().FindString("version_number"));
}

class WebstorePrivateExtensionInstallRequestBase : public ExtensionApiUnittest {
 public:
  using ExtensionInstallStatus = api::webstore_private::ExtensionInstallStatus;
  WebstorePrivateExtensionInstallRequestBase()
      : ExtensionApiUnittest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  WebstorePrivateExtensionInstallRequestBase(
      const WebstorePrivateExtensionInstallRequestBase&) = delete;
  WebstorePrivateExtensionInstallRequestBase& operator=(
      const WebstorePrivateExtensionInstallRequestBase&) = delete;

  std::string GenerateArgs(const char* id) {
    return base::StringPrintf(R"(["%s"])", id);
  }

  std::string GenerateArgs(const char* id, const char* manifest) {
    return base::StringPrintf(R"(["%s", "%s"])", id, manifest);
  }

  scoped_refptr<const Extension> CreateExtension(const ExtensionId& id) {
    return ExtensionBuilder("extension").SetID(id).Build();
  }

  void VerifyResponse(const ExtensionInstallStatus& expected_response,
                      const base::Value& actual_response) {
    ASSERT_TRUE(actual_response.is_string());
    EXPECT_EQ(ToString(expected_response), actual_response.GetString());
  }
};

class WebstorePrivateGetExtensionStatusTest
    : public WebstorePrivateExtensionInstallRequestBase {
 public:
  void SetUp() override {
    WebstorePrivateExtensionInstallRequestBase::SetUp();
    in_process_data_decoder_ =
        std::make_unique<data_decoder::test::InProcessDataDecoder>();
  }

 private:
  std::unique_ptr<data_decoder::test::InProcessDataDecoder>
      in_process_data_decoder_;
};

TEST_F(WebstorePrivateGetExtensionStatusTest, InvalidExtensionId) {
  auto function =
      base::MakeRefCounted<WebstorePrivateGetExtensionStatusFunction>();
  EXPECT_EQ(kInvalidId,
            RunFunctionAndReturnError(function.get(),
                                      GenerateArgs("invalid-extension-id")));
}

TEST_F(WebstorePrivateGetExtensionStatusTest, ExtensionEnabled) {
  ExtensionRegistry::Get(profile())->AddEnabled(CreateExtension(kExtensionId));
  auto function =
      base::MakeRefCounted<WebstorePrivateGetExtensionStatusFunction>();
  std::optional<base::Value> response =
      RunFunctionAndReturnValue(function.get(), GenerateArgs(kExtensionId));
  VerifyResponse(ExtensionInstallStatus::kEnabled, *response);
}

TEST_F(WebstorePrivateGetExtensionStatusTest, InvalidManifest) {
  auto function =
      base::MakeRefCounted<WebstorePrivateGetExtensionStatusFunction>();
  EXPECT_EQ(
      "Invalid manifest",
      RunFunctionAndReturnError(
          function.get(), GenerateArgs(kExtensionId, "invalid-manifest")));
}

TEST_F(WebstorePrivateGetExtensionStatusTest, ExtensionBlockedByManifestType) {
  SetExtensionSettings(kBlockedManifestTypeExtensionSettings, profile());
  auto function =
      base::MakeRefCounted<WebstorePrivateGetExtensionStatusFunction>();
  std::optional<base::Value> response = RunFunctionAndReturnValue(
      function.get(), GenerateArgs(kExtensionId, kExtensionManifest));
  VerifyResponse(ExtensionInstallStatus::kBlockedByPolicy, *response);
}

TEST_F(WebstorePrivateGetExtensionStatusTest, ExtensionBlockedByPermission) {
  SetExtensionSettings(kBlockedDownloadsPermissionsExtensionSettings,
                       profile());
  auto function =
      base::MakeRefCounted<WebstorePrivateGetExtensionStatusFunction>();
  std::optional<base::Value> response = RunFunctionAndReturnValue(
      function.get(), GenerateArgs(kExtensionId, kExtensionManifest));
  VerifyResponse(ExtensionInstallStatus::kBlockedByPolicy, *response);
}

TEST_F(WebstorePrivateGetExtensionStatusTest,
       ExtensionNotBlockedByOptionalPermission) {
  SetExtensionSettings(kBlockedAudioPermissionsExtensionSettings, profile());
  auto function =
      base::MakeRefCounted<WebstorePrivateGetExtensionStatusFunction>();
  std::optional<base::Value> response = RunFunctionAndReturnValue(
      function.get(), GenerateArgs(kExtensionId, kExtensionManifest));
  VerifyResponse(ExtensionInstallStatus::kInstallable, *response);
}

TEST_F(WebstorePrivateGetExtensionStatusTest, ExtensionCorrupted) {
  ExtensionRegistry::Get(profile())->AddDisabled(CreateExtension(kExtensionId));
  ExtensionPrefs::Get(profile())->SetExtensionDisabled(
      kExtensionId, disable_reason::DISABLE_CORRUPTED);
  auto function =
      base::MakeRefCounted<WebstorePrivateGetExtensionStatusFunction>();
  std::optional<base::Value> response = RunFunctionAndReturnValue(
      function.get(), GenerateArgs(kExtensionId, kExtensionManifest));
  VerifyResponse(ExtensionInstallStatus::kCorrupted, *response);
}

class SupervisedUserWebstorePrivateGetExtensionStatusTest
    : public WebstorePrivateGetExtensionStatusTest {
 public:
  SupervisedUserWebstorePrivateGetExtensionStatusTest() {
    std::vector<base::test::FeatureRef> enabled_features;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
    enabled_features.push_back(
        supervised_user::
            kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
    enabled_features.push_back(
        supervised_user::
            kEnableSupervisedUserSkipParentApprovalToInstallExtensions);
    enabled_features.push_back(
        supervised_user::kExposedParentalControlNeededForExtensionInstallation);
    feature_list_.InitWithFeatures(enabled_features, /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SupervisedUserWebstorePrivateGetExtensionStatusTest,
       ExtensionCustodianApprovalRequired) {
  profile()->SetIsSupervisedProfile(true);

  ExtensionRegistry::Get(profile())->AddDisabled(CreateExtension(kExtensionId));
  ExtensionPrefs::Get(profile())->SetExtensionDisabled(
      kExtensionId, disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);
  auto function =
      base::MakeRefCounted<WebstorePrivateGetExtensionStatusFunction>();
  std::optional<base::Value> response =
      RunFunctionAndReturnValue(function.get(), GenerateArgs(kExtensionId));
  VerifyResponse(ExtensionInstallStatus::kCustodianApprovalRequired, *response);
}

TEST_F(SupervisedUserWebstorePrivateGetExtensionStatusTest,
       ExtensionCustodianApprovalRequiredForInstallation) {
  profile()->SetIsSupervisedProfile(true);

  auto function =
      base::MakeRefCounted<WebstorePrivateGetExtensionStatusFunction>();
  std::optional<base::Value> response =
      RunFunctionAndReturnValue(function.get(), GenerateArgs(kExtensionId));

  ASSERT_FALSE(
      ExtensionRegistry::Get(profile())->GetInstalledExtension(kExtensionId));
  VerifyResponse(
      ExtensionInstallStatus::kCustodianApprovalRequiredForInstallation,
      *response);
}

class WebstorePrivateBeginInstallWithManifest3Test
    : public ExtensionApiUnittest {
 public:
  WebstorePrivateBeginInstallWithManifest3Test()
      : ExtensionApiUnittest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ExtensionApiUnittest::SetUp();
    ManagementAPI::GetFactoryInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildManagementApi));
    EventRouterFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildEventRouter));
    TestExtensionSystem* test_extension_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
    service_ = test_extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
  }

  void EnableExtensionRequest(bool enable) {
    profile()->GetTestingPrefService()->SetManagedPref(
        prefs::kCloudExtensionRequestEnabled,
        std::make_unique<base::Value>(enable));
  }

  void SetExtensionSettings(const std::string& settings_string) {
    std::optional<base::Value> settings =
        base::JSONReader::Read(settings_string);
    ASSERT_TRUE(settings);
    profile()->GetTestingPrefService()->SetManagedPref(
        pref_names::kExtensionManagement,
        base::Value::ToUniquePtrValue(std::move(*settings)));
  }

  std::string GenerateArgs(const char* id, const char* manifest) {
    return base::StringPrintf(R"([{"id":"%s", "manifest":"%s"}])", id,
                              manifest);
  }

  void VerifyUserCancelledFunctionResult(ExtensionFunction* function) {
    ASSERT_TRUE(function->GetResultListForTest());
    const base::Value& result = (*function->GetResultListForTest())[0];
    EXPECT_EQ("user_cancelled", result.GetString());
    EXPECT_EQ(kWebstoreUserCancelledError, function->GetError());
  }

  void VerifyBlockedByPolicyFunctionResult(
      WebstorePrivateBeginInstallWithManifest3Function* function,
      const std::u16string& expected_blocked_message) {
    ASSERT_TRUE(function->GetResultListForTest());
    const base::Value& result = (*function->GetResultListForTest())[0];
    EXPECT_EQ("blocked_by_policy", result.GetString());
    EXPECT_EQ(kWebstoreBlockByPolicy, function->GetError());
    EXPECT_EQ(expected_blocked_message,
              function->GetBlockedByPolicyErrorMessageForTesting());
  }

  scoped_refptr<const Extension> CreateExtension(const ExtensionId& id) {
    return ExtensionBuilder("extension").SetID(id).Build();
  }

  ExtensionService* extension_service() { return service_; }

 private:
  // This test does not create a root window. Because of this,
  // ScopedDisableRootChecking needs to be used (which disables the root window
  // check).
  test::ScopedDisableRootChecking disable_root_checking_;
  raw_ptr<ExtensionService, DanglingUntriaged> service_ = nullptr;
};

TEST_F(WebstorePrivateBeginInstallWithManifest3Test,
       RequestExtensionWithConfirmThenShowPendingDialog) {
  EnableExtensionRequest(true);
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

  VerifyPendingList({}, profile());

  // Confirm request dialog
  auto function =
      base::MakeRefCounted<WebstorePrivateBeginInstallWithManifest3Function>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  {
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);
    api_test_utils::RunFunction(function.get(),
                                GenerateArgs(kExtensionId, kExtensionManifest),
                                profile());
  }
  VerifyUserCancelledFunctionResult(function.get());
  VerifyPendingList({{kExtensionId, ExtensionRequestData(base::Time::Now())}},
                    profile());

  // Show pending request dialog which can only be canceled.
  function =
      base::MakeRefCounted<WebstorePrivateBeginInstallWithManifest3Function>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  {
    ScopedTestDialogAutoConfirm auto_cancel(
        ScopedTestDialogAutoConfirm::CANCEL);
    api_test_utils::RunFunction(function.get(),
                                GenerateArgs(kExtensionId, kExtensionManifest),
                                profile());
  }
  VerifyUserCancelledFunctionResult(function.get());
  VerifyPendingList({{kExtensionId, ExtensionRequestData(base::Time::Now())}},
                    profile());
}

TEST_F(WebstorePrivateBeginInstallWithManifest3Test,
       RequestExtensionWithCancel) {
  EnableExtensionRequest(true);
  VerifyPendingList({}, profile());

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto function =
      base::MakeRefCounted<WebstorePrivateBeginInstallWithManifest3Function>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  ScopedTestDialogAutoConfirm auto_cancel(ScopedTestDialogAutoConfirm::CANCEL);

  api_test_utils::RunFunction(function.get(),
                              GenerateArgs(kExtensionId, kExtensionManifest),
                              profile());
  VerifyUserCancelledFunctionResult(function.get());
  VerifyPendingList({}, profile());
}

TEST_F(WebstorePrivateBeginInstallWithManifest3Test,
       RequestExtensionWithJustification) {
  EnableExtensionRequest(true);
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

  VerifyPendingList({}, profile());

  auto function =
      base::MakeRefCounted<WebstorePrivateBeginInstallWithManifest3Function>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  {
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);
    auto_confirm.set_justification(kFakeJustification);
    api_test_utils::RunFunction(function.get(),
                                GenerateArgs(kExtensionId, kExtensionManifest),
                                profile());
  }
  // Even though the ACCEPT button was selected above, the extension request
  // dialog results in user_cancelled.
  VerifyUserCancelledFunctionResult(function.get());
  VerifyPendingList({{kExtensionId, ExtensionRequestData(base::Time::Now(),
                                                         kFakeJustification)}},
                    profile());
}

TEST_F(WebstorePrivateBeginInstallWithManifest3Test,
       RequestExtensionWithJustificationAndCancel) {
  EnableExtensionRequest(true);
  VerifyPendingList({}, profile());

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto function =
      base::MakeRefCounted<WebstorePrivateBeginInstallWithManifest3Function>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  {
    ScopedTestDialogAutoConfirm auto_cancel(
        ScopedTestDialogAutoConfirm::CANCEL);
    auto_cancel.set_justification(kFakeJustification);

    api_test_utils::RunFunction(function.get(),
                                GenerateArgs(kExtensionId, kExtensionManifest),
                                profile());
  }
  VerifyUserCancelledFunctionResult(function.get());
  VerifyPendingList({}, profile());
}

TEST_F(WebstorePrivateBeginInstallWithManifest3Test,
       NormalInstallIfRequestExtensionIsDisabled) {
  EnableExtensionRequest(true);
  VerifyPendingList({}, profile());

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto function =
      base::MakeRefCounted<WebstorePrivateBeginInstallWithManifest3Function>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  {
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);
    api_test_utils::RunFunction(function.get(),
                                GenerateArgs(kExtensionId, kExtensionManifest),
                                profile());
  }
  VerifyPendingList({{kExtensionId, ExtensionRequestData(base::Time::Now())}},
                    profile());

  // Show install prompt dialog if extension request feature is disabled.
  EnableExtensionRequest(false);
  function =
      base::MakeRefCounted<WebstorePrivateBeginInstallWithManifest3Function>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  {
    // Successfully confirm the install prompt and the API returns an empty
    // string without error.
    ScopedTestDialogAutoConfirm auto_cancel(
        ScopedTestDialogAutoConfirm::ACCEPT);
    std::optional<base::Value> response = RunFunctionAndReturnValue(
        function.get(), GenerateArgs(kExtensionId, kExtensionManifest));
    ASSERT_TRUE(response);
    ASSERT_TRUE(response->is_string());
    EXPECT_TRUE(response->GetString().empty());
  }

  // Pending list is not changed.
  VerifyPendingList({{kExtensionId, ExtensionRequestData(base::Time::Now())}},
                    profile());
}

TEST_F(WebstorePrivateBeginInstallWithManifest3Test, BlockedByPolicy) {
  SetExtensionSettings(kBlockAllExtensionSettings);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto function =
      base::MakeRefCounted<WebstorePrivateBeginInstallWithManifest3Function>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  api_test_utils::RunFunction(function.get(),
                              GenerateArgs(kExtensionId, kExtensionManifest),
                              profile());
  VerifyBlockedByPolicyFunctionResult(
      function.get(), u"From your administrator: This extension is blocked.");
}

TEST_F(WebstorePrivateBeginInstallWithManifest3Test,
       BlockedByPolicyWithExtensionRequest) {
  SetExtensionSettings(kBlockOneExtensionSettings);
  EnableExtensionRequest(true);
  VerifyPendingList({}, profile());

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto function =
      base::MakeRefCounted<WebstorePrivateBeginInstallWithManifest3Function>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  api_test_utils::RunFunction(function.get(),
                              GenerateArgs(kExtensionId, kExtensionManifest),
                              profile());
  VerifyPendingList({}, profile());
  VerifyBlockedByPolicyFunctionResult(function.get(), std::u16string());
}

TEST_F(WebstorePrivateBeginInstallWithManifest3Test,
       ExtensionBlockedByManifestType) {
  SetExtensionSettings(kBlockedManifestTypeExtensionSettings);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto function =
      base::MakeRefCounted<WebstorePrivateBeginInstallWithManifest3Function>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  api_test_utils::RunFunction(function.get(),
                              GenerateArgs(kExtensionId, kExtensionManifest),
                              profile());
  VerifyBlockedByPolicyFunctionResult(function.get(), std::u16string());
}

TEST_F(WebstorePrivateBeginInstallWithManifest3Test,
       ExtensionBlockedByPermission) {
  SetExtensionSettings(kBlockedDownloadsPermissionsExtensionSettings);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto function =
      base::MakeRefCounted<WebstorePrivateBeginInstallWithManifest3Function>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  api_test_utils::RunFunction(function.get(),
                              GenerateArgs(kExtensionId, kExtensionManifest),
                              profile());
  VerifyBlockedByPolicyFunctionResult(function.get(), std::u16string());
}

TEST_F(WebstorePrivateBeginInstallWithManifest3Test,
       ExtensionNotBlockedByOptionalPermission) {
  SetExtensionSettings(kBlockedAudioPermissionsExtensionSettings);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto function =
      base::MakeRefCounted<WebstorePrivateBeginInstallWithManifest3Function>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  std::optional<base::Value> response = RunFunctionAndReturnValue(
      function.get(), GenerateArgs(kExtensionId, kExtensionManifest));
  // The API returns an empty string on success.
  ASSERT_TRUE(response);
  ASSERT_TRUE(response->is_string());
  EXPECT_EQ(std::string(), response->GetString());
}

TEST_F(WebstorePrivateBeginInstallWithManifest3Test,
       ProfileDeletedBeforeCompleteInstall) {
  const std::string profile_name = "deleted_before_complete_install";
  TestingProfile* const test_profile =
      profile_manager()->CreateTestingProfile(profile_name);
  ASSERT_TRUE(test_profile);
  // There should be no pending approvals.
  EXPECT_EQ(WebstorePrivateApi::GetPendingApprovalsCountForTesting(), 0);
  {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(test_profile,
                                                          nullptr);
    auto function = base::MakeRefCounted<
        WebstorePrivateBeginInstallWithManifest3Function>();
    function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);

    function->set_extension(extension());
    auto response = api_test_utils::RunFunctionAndReturnSingleResult(
        function.get(), GenerateArgs(kExtensionId, kExtensionManifest),
        test_profile);
    // The API returns an empty string on success.
    ASSERT_TRUE(response);
    ASSERT_TRUE(response->is_string());
    EXPECT_EQ(response->GetString(), "");
    // Running the function creates a pending approval.
    EXPECT_EQ(WebstorePrivateApi::GetPendingApprovalsCountForTesting(), 1);
  }
  // Deleting the Profile should remove the pending approval.
  profile_manager()->DeleteTestingProfile(profile_name);
  EXPECT_EQ(WebstorePrivateApi::GetPendingApprovalsCountForTesting(), 0);
}

struct FrictionDialogTestCase {
  std::string test_name;
  bool esb_user;
  std::string esb_allowlist;
  bool expected_friction_shown;
  ScopedTestDialogAutoConfirm::AutoConfirm dialog_action =
      ScopedTestDialogAutoConfirm::ACCEPT;
};

std::ostream& operator<<(std::ostream& out,
                         const FrictionDialogTestCase& test_case) {
  out << test_case.test_name;
  return out;
}

const FrictionDialogTestCase kFrictionDialogTestCases[] = {
    {/*test_name=*/"EsbUserAndNotAllowlisted",
     /*esb_user=*/true,
     /*esb_allowlist=*/"false",
     /*expected_friction_shown=*/true},

    {/*test_name=*/"EsbUserAndAllowlisted",
     /*esb_user=*/true,
     /*esb_allowlist=*/"true",
     /*expected_friction_shown=*/false},

    {/*test_name=*/"EsbUserAndUndefined",
     /*esb_user=*/true,
     /*esb_allowlist=*/"undefined",
     /*expected_friction_shown=*/false},
    {/*test_name=*/"NonEsbUserAndNotAllowlisted",
     /*esb_user=*/false,
     /*esb_allowlist=*/"false",
     /*expected_friction_shown=*/false},

    {/*test_name=*/"CancelFrictionDialog",
     /*esb_user=*/true,
     /*esb_allowlist=*/"false",
     /*expected_friction_shown=*/true,
     /*dialog_action=*/ScopedTestDialogAutoConfirm::CANCEL}};

class WebstorePrivateBeginInstallWithManifest3FrictionDialogTest
    : public WebstorePrivateBeginInstallWithManifest3Test,
      public testing::WithParamInterface<FrictionDialogTestCase> {
 public:
  WebstorePrivateBeginInstallWithManifest3FrictionDialogTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kSafeBrowsingCrxAllowlistShowWarnings);
  }

  void SetUp() override {
    WebstorePrivateBeginInstallWithManifest3Test::SetUp();

    // Clear the pending approvals. Leftover approvals can stay pending when
    // testing the `webstorePrivate.beginInstallWithManifest3` function
    // without calling `webstorePrivate.completeInstall`.
    WebstorePrivateApi::ClearPendingApprovalsForTesting();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(WebstorePrivateBeginInstallWithManifest3FrictionDialogTest,
       FrictionDialogTests) {
  FrictionDialogTestCase test_case = GetParam();

  if (test_case.esb_user) {
    // Enable Enhanced Protection
    safe_browsing::SetSafeBrowsingState(
        profile()->GetPrefs(),
        safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  }
  extension_service()->Init();

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  auto function =
      base::MakeRefCounted<WebstorePrivateBeginInstallWithManifest3Function>();
  function->SetRenderFrameHost(web_contents->GetPrimaryMainFrame());
  ScopedTestDialogAutoConfirm auto_confirm(test_case.dialog_action);

  std::string args =
      test_case.esb_allowlist == "undefined"
          ? base::StringPrintf(R"([{"id":"%s", "manifest":"%s"}])",
                               kExtensionId, kExtensionManifest)
          : base::StringPrintf(
                R"([{"id":"%s", "manifest":"%s", "esbAllowlist":%s}])",
                kExtensionId, kExtensionManifest,
                test_case.esb_allowlist.c_str());

  if (test_case.dialog_action == ScopedTestDialogAutoConfirm::ACCEPT) {
    std::optional<base::Value> response =
        RunFunctionAndReturnValue(function.get(), args);

    // The API returns empty string when extension is installed successfully.
    ASSERT_TRUE(response);
    ASSERT_TRUE(response->is_string());
    EXPECT_EQ(std::string(), response->GetString());
  } else {
    api_test_utils::RunFunction(function.get(), args, profile());
    VerifyUserCancelledFunctionResult(function.get());
  }

  EXPECT_EQ(test_case.expected_friction_shown,
            function->GetFrictionDialogShownForTesting());

  std::unique_ptr<WebstoreInstaller::Approval> approval =
      WebstorePrivateApi::PopApprovalForTesting(profile(), kExtensionId);
  if (test_case.dialog_action == ScopedTestDialogAutoConfirm::ACCEPT) {
    ASSERT_TRUE(approval);
    EXPECT_EQ(test_case.expected_friction_shown,
              approval->bypassed_safebrowsing_friction);
  } else {
    EXPECT_FALSE(approval);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WebstorePrivateBeginInstallWithManifest3FrictionDialogTest,
    testing::ValuesIn(kFrictionDialogTestCases),
    [](const testing::TestParamInfo<FrictionDialogTestCase>& info) {
      return info.param.test_name;
    });

// A test suite to be used with the MV2 deprecation experiments.
class WebstorePrivateManifestV2DeprecationUnitTest
    : public ExtensionApiUnittest,
      public testing::WithParamInterface<MV2ExperimentStage> {
 public:
  WebstorePrivateManifestV2DeprecationUnitTest();
  ~WebstorePrivateManifestV2DeprecationUnitTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

WebstorePrivateManifestV2DeprecationUnitTest::
    WebstorePrivateManifestV2DeprecationUnitTest() {
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
  switch (GetParam()) {
    case MV2ExperimentStage::kNone:
      disabled_features.push_back(
          extensions_features::kExtensionManifestV2DeprecationWarning);
      disabled_features.push_back(
          extensions_features::kExtensionManifestV2Disabled);
      disabled_features.push_back(
          extensions_features::kExtensionManifestV2Unsupported);
      break;
    case MV2ExperimentStage::kWarning:
      enabled_features.push_back(
          extensions_features::kExtensionManifestV2DeprecationWarning);
      disabled_features.push_back(
          extensions_features::kExtensionManifestV2Disabled);
      disabled_features.push_back(
          extensions_features::kExtensionManifestV2Unsupported);
      break;
    case MV2ExperimentStage::kDisableWithReEnable:
      enabled_features.push_back(
          extensions_features::kExtensionManifestV2Disabled);
      disabled_features.push_back(
          extensions_features::kExtensionManifestV2DeprecationWarning);
      disabled_features.push_back(
          extensions_features::kExtensionManifestV2Unsupported);
      break;
    case MV2ExperimentStage::kUnsupported:
      enabled_features.push_back(
          extensions_features::kExtensionManifestV2Unsupported);
      disabled_features.push_back(
          extensions_features::kExtensionManifestV2Disabled);
      disabled_features.push_back(
          extensions_features::kExtensionManifestV2DeprecationWarning);
      break;
  }

  feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    WebstorePrivateManifestV2DeprecationUnitTest,
    testing::Values(MV2ExperimentStage::kNone,
                    MV2ExperimentStage::kWarning,
                    MV2ExperimentStage::kDisableWithReEnable,
                    MV2ExperimentStage::kUnsupported),
    [](const testing::TestParamInfo<MV2ExperimentStage>& info) {
      switch (info.param) {
        case MV2ExperimentStage::kNone:
          return "ExperimentDisabled";
        case MV2ExperimentStage::kWarning:
          return "WarningExperiment";
        case MV2ExperimentStage::kDisableWithReEnable:
          return "DisableExperiment";
        case MV2ExperimentStage::kUnsupported:
          return "UnsupportedExperiment";
      }
    });

// Tests the behavior of the webstorePrivate.getMV2DeprecationStatus() function.
TEST_P(WebstorePrivateManifestV2DeprecationUnitTest,
       TestGetMV2DeprecationStatus) {
  auto function =
      base::MakeRefCounted<WebstorePrivateGetMV2DeprecationStatusFunction>();
  std::optional<base::Value> response =
      api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), /*args*/ "[]", browser()->profile());
  ASSERT_TRUE(response);

  std::string expected;
  switch (GetParam()) {
    case MV2ExperimentStage::kNone:
      expected = "inactive";
      break;
    case MV2ExperimentStage::kWarning:
      expected = "warning";
      break;
    case MV2ExperimentStage::kDisableWithReEnable:
      expected = "soft_disable";
      break;
    case MV2ExperimentStage::kUnsupported:
      expected = "hard_disable";
      break;
  }

  EXPECT_EQ(expected, *response);
}

}  // namespace extensions
