// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cryptotoken_private/cryptotoken_private_api.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/sessions/content/session_tab_helper.h"
#include "crypto/sha2.h"
#include "device/fido/features.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace extensions {

namespace {

bool GetSingleBooleanResult(ExtensionFunction* function, bool* result) {
  const base::Value::List* result_list = function->GetResultList();
  if (!result_list) {
    ADD_FAILURE() << "Function has no result list.";
    return false;
  }

  if (result_list->size() != 1u) {
    ADD_FAILURE() << "Invalid number of results.";
    return false;
  }

  if (!(*result_list)[0].is_bool()) {
    ADD_FAILURE() << "Result is not boolean.";
    return false;
  }

  *result = (*result_list)[0].GetBool();
  return true;
}

class CryptoTokenPrivateApiTest : public extensions::ExtensionApiUnittest {
 public:
  CryptoTokenPrivateApiTest() {}
  ~CryptoTokenPrivateApiTest() override {}

 protected:
  bool GetCanOriginAssertAppIdResult(const std::string& origin,
                                     const std::string& app_id,
                                     bool* out_result) {
    auto function = base::MakeRefCounted<
        api::CryptotokenPrivateCanOriginAssertAppIdFunction>();
    function->set_has_callback(true);

    auto args = std::make_unique<base::ListValue>();
    args->Append(origin);
    args->Append(app_id);

    if (!extension_function_test_utils::RunFunction(
            function.get(), std::move(args), browser(), api_test_utils::NONE)) {
      return false;
    }

    return GetSingleBooleanResult(function.get(), out_result);
  }

  bool GetAppIdHashInEnterpriseContext(const std::string& app_id,
                                       bool* out_result) {
    auto function = base::MakeRefCounted<
        api::CryptotokenPrivateIsAppIdHashInEnterpriseContextFunction>();
    function->set_has_callback(true);

    auto args = std::make_unique<base::Value>(base::Value::Type::LIST);
    args->Append(
        base::Value(base::Value::BlobStorage(app_id.begin(), app_id.end())));

    if (!extension_function_test_utils::RunFunction(
            function.get(), base::ListValue::From(std::move(args)), browser(),
            api_test_utils::NONE)) {
      return false;
    }

    return GetSingleBooleanResult(function.get(), out_result);
  }
};

TEST_F(CryptoTokenPrivateApiTest, CanOriginAssertAppId) {
  std::string origin1("https://www.example.com");

  bool result;
  ASSERT_TRUE(GetCanOriginAssertAppIdResult(origin1, origin1, &result));
  EXPECT_TRUE(result);

  std::string same_origin_appid("https://www.example.com/appId");
  ASSERT_TRUE(
      GetCanOriginAssertAppIdResult(origin1, same_origin_appid, &result));
  EXPECT_TRUE(result);
  std::string same_etld_plus_one_appid("https://appid.example.com/appId");
  ASSERT_TRUE(GetCanOriginAssertAppIdResult(origin1, same_etld_plus_one_appid,
                                            &result));
  EXPECT_TRUE(result);
  std::string different_etld_plus_one_appid("https://www.different.com/appId");
  ASSERT_TRUE(GetCanOriginAssertAppIdResult(
      origin1, different_etld_plus_one_appid, &result));
  EXPECT_FALSE(result);

  // For legacy purposes, google.com is allowed to use certain appIds hosted at
  // gstatic.com.
  // TODO(juanlang): remove once the legacy constraints are removed.
  std::string google_origin("https://accounts.google.com");
  std::string gstatic_appid("https://www.gstatic.com/securitykey/origins.json");
  ASSERT_TRUE(
      GetCanOriginAssertAppIdResult(google_origin, gstatic_appid, &result));
  EXPECT_TRUE(result);
  // Not all gstatic urls are allowed, just those specifically allowlisted.
  std::string gstatic_otherurl("https://www.gstatic.com/foobar");
  ASSERT_TRUE(
      GetCanOriginAssertAppIdResult(google_origin, gstatic_otherurl, &result));
  EXPECT_FALSE(result);
}

TEST_F(CryptoTokenPrivateApiTest, IsAppIdHashInEnterpriseContext) {
  const std::string example_com("https://example.com/");
  const std::string example_com_hash(crypto::SHA256HashString(example_com));
  const std::string rp_id_hash(crypto::SHA256HashString("example.com"));
  const std::string foo_com_hash(crypto::SHA256HashString("https://foo.com/"));

  bool result;
  ASSERT_TRUE(GetAppIdHashInEnterpriseContext(example_com_hash, &result));
  EXPECT_FALSE(result);
  ASSERT_TRUE(GetAppIdHashInEnterpriseContext(foo_com_hash, &result));
  EXPECT_FALSE(result);
  ASSERT_TRUE(GetAppIdHashInEnterpriseContext(rp_id_hash, &result));
  EXPECT_FALSE(result);

  base::Value::List permitted_list;
  permitted_list.Append(example_com);
  profile()->GetPrefs()->SetList(prefs::kSecurityKeyPermitAttestation,
                                 std::move(permitted_list));

  ASSERT_TRUE(GetAppIdHashInEnterpriseContext(example_com_hash, &result));
  EXPECT_TRUE(result);
  ASSERT_TRUE(GetAppIdHashInEnterpriseContext(foo_com_hash, &result));
  EXPECT_FALSE(result);
  ASSERT_TRUE(GetAppIdHashInEnterpriseContext(rp_id_hash, &result));
  EXPECT_FALSE(result);
}

TEST_F(CryptoTokenPrivateApiTest, RecordRegisterRequest) {
  const GURL url("https://example.com/signin");
  AddTab(browser(), url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  const int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();

  page_load_metrics::PageLoadMetricsTestWaiter web_feature_waiter(web_contents);
  web_feature_waiter.AddWebFeatureExpectation(
      blink::mojom::WebFeature::kU2FCryptotokenRegister);
  // Force the metrics waiter to attach.
  NavigateAndCommitActiveTab(url);

  auto function = base::MakeRefCounted<
      api::CryptotokenPrivateRecordRegisterRequestFunction>();
  auto args = std::make_unique<base::ListValue>();
  args->Append(tab_id);
  args->Append(0 /* top-level frame */);
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(), base::ListValue::From(std::move(args)), browser(),
      api_test_utils::NONE));
  ASSERT_EQ(function->GetResultList()->size(), 0u);

  web_feature_waiter.Wait();
}

TEST_F(CryptoTokenPrivateApiTest, RecordSignRequest) {
  const GURL url("https://example.com/signin");
  AddTab(browser(), url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  const int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();

  page_load_metrics::PageLoadMetricsTestWaiter web_feature_waiter(web_contents);
  web_feature_waiter.AddWebFeatureExpectation(
      blink::mojom::WebFeature::kU2FCryptotokenSign);
  // Force the metrics waiter to attach.
  NavigateAndCommitActiveTab(url);

  auto function =
      base::MakeRefCounted<api::CryptotokenPrivateRecordSignRequestFunction>();
  auto args = std::make_unique<base::ListValue>();
  args->Append(tab_id);
  args->Append(0 /* top-level frame */);
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(), base::ListValue::From(std::move(args)), browser(),
      api_test_utils::NONE));
  ASSERT_EQ(function->GetResultList()->size(), 0u);

  web_feature_waiter.Wait();
}
}  // namespace

class CryptoTokenPermissionTest : public ExtensionApiUnittest {
 public:
  CryptoTokenPermissionTest() = default;

  CryptoTokenPermissionTest(const CryptoTokenPermissionTest&) = delete;
  CryptoTokenPermissionTest& operator=(const CryptoTokenPermissionTest&) =
      delete;

  ~CryptoTokenPermissionTest() override = default;

  void SetUp() override {
    ExtensionApiUnittest::SetUp();
    feature_list_.InitAndEnableFeature(extensions_features::kU2FSecurityKeyAPI);
    const GURL url("http://example.com");
    AddTab(browser(), url);

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    tab_id_ = sessions::SessionTabHelper::IdForTab(web_contents).id();
    permissions::PermissionRequestManager::CreateForWebContents(web_contents);
    prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(
            permissions::PermissionRequestManager::FromWebContents(
                web_contents));
  }

  void TearDown() override {
    prompt_factory_.reset();
    ExtensionApiUnittest::TearDown();
  }

 protected:
  // CanAppIdGetAttestation calls the cryptotoken private API of the same name
  // for |app_id| and sets |*out_result| to the result. If |bubble_action| is
  // not |NONE| then it waits for the permissions prompt to be shown and
  // performs the given action. Otherwise, the call is expected to be
  // synchronous.
  bool CanAppIdGetAttestation(
      const std::string& app_id,
      permissions::PermissionRequestManager::AutoResponseType bubble_action,
      bool* out_result) {
    if (bubble_action != permissions::PermissionRequestManager::NONE) {
      prompt_factory_->set_response_type(bubble_action);
      prompt_factory_->DocumentOnLoadCompletedInPrimaryMainFrame();
    }

    auto function = base::MakeRefCounted<
        api::CryptotokenPrivateCanAppIdGetAttestationFunction>();
    function->set_has_callback(true);

    base::Value::Dict dict;
    dict.Set("appId", app_id);
    dict.Set("tabId", tab_id_);
    dict.Set("frameId", -1);  // Ignored.
    dict.Set("origin", app_id);
    auto args = std::make_unique<base::Value>(base::Value::Type::LIST);
    args->Append(base::Value(std::move(dict)));
    auto args_list = base::ListValue::From(std::move(args));

    extension_function_test_utils::RunFunction(
        function.get(), std::move(args_list), browser(), api_test_utils::NONE);

    return GetSingleBooleanResult(function.get(), out_result);
  }

  // CanMakeU2fApiRequest calls the cryptotoken private API of the same name
  // for |origin| and sets |*out_result| to the result. If |bubble_action| is
  // not |NONE| then it waits for the permissions prompt to be shown and
  // performs the given action. Otherwise, the call is expected to be
  // synchronous.
  bool CanMakeU2fApiRequest(
      const std::string& origin,
      permissions::PermissionRequestManager::AutoResponseType bubble_action,
      bool* out_result) {
    if (bubble_action != permissions::PermissionRequestManager::NONE) {
      prompt_factory_->set_response_type(bubble_action);
      prompt_factory_->DocumentOnLoadCompletedInPrimaryMainFrame();
    }

    auto function = base::MakeRefCounted<
        api::CryptotokenPrivateCanMakeU2fApiRequestFunction>();
    function->set_has_callback(true);

    base::Value::Dict dict;
    dict.Set("appId", origin);
    dict.Set("tabId", tab_id_);
    dict.Set("frameId", 0 /* main frame */);
    dict.Set("origin", origin);
    auto args = std::make_unique<base::Value>(base::Value::Type::LIST);
    args->Append(base::Value(std::move(dict)));
    auto args_list = base::ListValue::From(std::move(args));

    extension_function_test_utils::RunFunction(
        function.get(), std::move(args_list), browser(), api_test_utils::NONE);

    return GetSingleBooleanResult(function.get(), out_result);
  }

 private:
  int tab_id_ = -1;
  std::unique_ptr<permissions::MockPermissionPromptFactory> prompt_factory_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(CryptoTokenPermissionTest, AttestationPrompt) {
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/1225335) This test is failing on WIN10_20H2.
  if (base::win::OSInfo::GetInstance()->version() >=
      base::win::Version::WIN10_20H2)
    return;
#endif

  const std::vector<permissions::PermissionRequestManager::AutoResponseType>
      actions = {
          permissions::PermissionRequestManager::ACCEPT_ALL,
          permissions::PermissionRequestManager::DENY_ALL,
          permissions::PermissionRequestManager::DISMISS,
      };

  for (const auto& action : actions) {
    SCOPED_TRACE(action);

    bool result = false;
    ASSERT_TRUE(CanAppIdGetAttestation("https://test.com", action, &result));
    // The result should only be positive if the user accepted the permissions
    // prompt.
    EXPECT_EQ(action == permissions::PermissionRequestManager::ACCEPT_ALL,
              result);
  }
}

TEST_F(CryptoTokenPermissionTest, PolicyOverridesAttestationPrompt) {
  const std::string example_com("https://example.com");
  base::Value::List permitted_list;
  permitted_list.Append(example_com);
  profile()->GetPrefs()->SetList(prefs::kSecurityKeyPermitAttestation,
                                 std::move(permitted_list));

  // If an appId is configured by enterprise policy then attestation requests
  // should be permitted without showing a prompt.
  bool result = false;
  ASSERT_TRUE(CanAppIdGetAttestation(
      example_com, permissions::PermissionRequestManager::NONE, &result));
  EXPECT_TRUE(result);
}

TEST_F(CryptoTokenPermissionTest, RequestPrompt) {
  const std::vector<permissions::PermissionRequestManager::AutoResponseType>
      actions = {
          permissions::PermissionRequestManager::ACCEPT_ALL,
          permissions::PermissionRequestManager::DENY_ALL,
          permissions::PermissionRequestManager::DISMISS,
      };

  for (const auto& action : actions) {
    SCOPED_TRACE(action);

    bool result = false;
    ASSERT_TRUE(CanMakeU2fApiRequest("https://test.com", action, &result));
    // The result should only be positive if the user accepted the permissions
    // prompt.
    EXPECT_EQ(action == permissions::PermissionRequestManager::ACCEPT_ALL,
              result);
  }
}

TEST_F(CryptoTokenPermissionTest, FeatureFlagOverridesRequestPrompt) {
  // Disabling the permission prompt feature flag should suppress it.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(device::kU2fPermissionPrompt);
  bool result = false;
  ASSERT_TRUE(CanMakeU2fApiRequest("https://test.com",
                                   permissions::PermissionRequestManager::NONE,
                                   &result));
  EXPECT_TRUE(result);
}

}  // namespace extensions
