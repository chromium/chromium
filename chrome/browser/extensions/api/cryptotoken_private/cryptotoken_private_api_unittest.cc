// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cryptotoken_private/cryptotoken_private_api.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/permission_bubble/mock_permission_prompt_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "crypto/sha2.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

bool GetSingleBooleanResult(ExtensionFunction* function, bool* result) {
  const base::ListValue* result_list = function->GetResultList();
  if (!result_list) {
    ADD_FAILURE() << "Function has no result list.";
    return false;
  }

  if (result_list->GetSize() != 1u) {
    ADD_FAILURE() << "Invalid number of results.";
    return false;
  }

  if (!result_list->GetBoolean(0, result)) {
    ADD_FAILURE() << "Result is not boolean.";
    return false;
  }

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
    args->AppendString(origin);
    args->AppendString(app_id);

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
  // Not all gstatic urls are allowed, just those specifically whitelisted.
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

  base::Value::ListStorage permitted_list;
  permitted_list.emplace_back(example_com);
  profile()->GetPrefs()->Set(prefs::kSecurityKeyPermitAttestation,
                             base::Value(permitted_list));

  ASSERT_TRUE(GetAppIdHashInEnterpriseContext(example_com_hash, &result));
  EXPECT_TRUE(result);
  ASSERT_TRUE(GetAppIdHashInEnterpriseContext(foo_com_hash, &result));
  EXPECT_FALSE(result);
  ASSERT_TRUE(GetAppIdHashInEnterpriseContext(rp_id_hash, &result));
  EXPECT_FALSE(result);
}

}  // namespace

class CryptoTokenPermissionTest : public ExtensionApiUnittest {
 public:
  CryptoTokenPermissionTest() = default;
  ~CryptoTokenPermissionTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures({features::kSecurityKeyAttestationPrompt},
                                   {});

    ExtensionApiUnittest::SetUp();
    const GURL url("http://example.com");
    AddTab(browser(), url);

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    tab_id_ = SessionTabHelper::IdForTab(web_contents).id();
    PermissionRequestManager::CreateForWebContents(web_contents);
    prompt_factory_ = std::make_unique<MockPermissionPromptFactory>(
        PermissionRequestManager::FromWebContents(web_contents));
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
      PermissionRequestManager::AutoResponseType bubble_action,
      bool* out_result) {
    if (bubble_action != PermissionRequestManager::NONE) {
      prompt_factory_->set_response_type(bubble_action);
      prompt_factory_->DocumentOnLoadCompletedInMainFrame();
    }

    auto function = base::MakeRefCounted<
        api::CryptotokenPrivateCanAppIdGetAttestationFunction>();
    function->set_has_callback(true);

    base::Value::DictStorage dict;
    dict.emplace("appId", std::make_unique<base::Value>(app_id));
    dict.emplace("tabId", std::make_unique<base::Value>(tab_id_));
    dict.emplace("origin", std::make_unique<base::Value>(app_id));
    auto args = std::make_unique<base::Value>(base::Value::Type::LIST);
    args->Append(base::Value(std::move(dict)));
    auto args_list = base::ListValue::From(std::move(args));

    extension_function_test_utils::RunFunction(
        function.get(), std::move(args_list), browser(), api_test_utils::NONE);

    return GetSingleBooleanResult(function.get(), out_result);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  int tab_id_ = -1;
  std::unique_ptr<MockPermissionPromptFactory> prompt_factory_;

  DISALLOW_COPY_AND_ASSIGN(CryptoTokenPermissionTest);
};

TEST_F(CryptoTokenPermissionTest, Prompt) {
  const std::vector<PermissionRequestManager::AutoResponseType> actions = {
      PermissionRequestManager::ACCEPT_ALL, PermissionRequestManager::DENY_ALL,
      PermissionRequestManager::DISMISS,
  };

  for (const auto& action : actions) {
    SCOPED_TRACE(action);

    bool result = false;
    ASSERT_TRUE(CanAppIdGetAttestation("https://test.com", action, &result));
    // The result should only be positive if the user accepted the permissions
    // prompt.
    EXPECT_EQ(action == PermissionRequestManager::ACCEPT_ALL, result);
  }
}

TEST_F(CryptoTokenPermissionTest, PolicyOverridesPrompt) {
  const std::string example_com("https://example.com");
  base::Value::ListStorage permitted_list;
  permitted_list.emplace_back(example_com);
  profile()->GetPrefs()->Set(prefs::kSecurityKeyPermitAttestation,
                             base::Value(permitted_list));

  // If an appId is configured by enterprise policy then attestation requests
  // should be permitted without showing a prompt.
  bool result = false;
  ASSERT_TRUE(CanAppIdGetAttestation(example_com,
                                     PermissionRequestManager::NONE, &result));
  EXPECT_TRUE(result);
}

}  // namespace extensions
