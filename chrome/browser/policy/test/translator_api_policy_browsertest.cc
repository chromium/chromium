// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/test/test_util.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features_generated.h"

using on_device_translation::CreateFakeDictionaryData;
using on_device_translation::LanguagePackKey;
using on_device_translation::MockComponentManager;
using on_device_translation::TestCanTranslate;
using on_device_translation::TestCreateTranslator;
using on_device_translation::TestSimpleTranslationWorks;

namespace policy {

// Test for TranslatorAPIAllowed policy.
class TranslatorAPIPolicyTest : public PolicyTest {
 public:
  TranslatorAPIPolicyTest() {
    // Need to enable the feature to test the policy.
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kEnableTranslationAPI);
    CHECK(tmp_dir_.CreateUniqueTempDir());
  }
  ~TranslatorAPIPolicyTest() override = default;

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    base::FilePath test_data_dir;
    GetTestDataDirectory(&test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());

    mock_component_manager_ =
        std::make_unique<MockComponentManager>(GetTempDir());
    // Install the mock TranslateKit component.
    mock_component_manager_->InstallMockTranslateKitComponent();
    // Install the mock language pack.
    mock_component_manager_->InstallMockLanguagePack(
        LanguagePackKey::kEn_Ja, CreateFakeDictionaryData("en", "ja"));
  }
  void TearDownOnMainThread() override { mock_component_manager_.reset(); }

 protected:
  const base::FilePath& GetTempDir() { return tmp_dir_.GetPath(); }

  MockComponentManager& mock_component_manager() {
    CHECK(mock_component_manager_);
    return *mock_component_manager_;
  }

  // Sets the TranslatorAPIAllowed policy.
  void SetTranslatorAPIAllowedPolicy(bool enabled) {
    PolicyMap policies;
    policies.Set(key::kTranslatorAPIAllowed, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(enabled),
                 nullptr);
    UpdateProviderPolicy(policies);
  }

  // Navigates to an empty page.
  void NavigateToEmptyPage() {
    CHECK(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/empty.html")));
  }

 private:
  base::ScopedTempDir tmp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MockComponentManager> mock_component_manager_;
};

// Test that the default value of the policy is allowed.
IN_PROC_BROWSER_TEST_F(TranslatorAPIPolicyTest, DefaultAllowed) {
  NavigateToEmptyPage();
  TestSimpleTranslationWorks(browser(), "en", "ja");
  TestCanTranslate(browser(), "en", "ja", "readily");
}

// Test that set the policy to false will disallow the API.
IN_PROC_BROWSER_TEST_F(TranslatorAPIPolicyTest, Disallow) {
  NavigateToEmptyPage();
  SetTranslatorAPIAllowedPolicy(false);
  TestCreateTranslator(browser(), "en", "ja",
                       "NotSupportedError: Unable to create translator for the "
                       "given source and target language.");
  TestCanTranslate(browser(), "en", "ja", "no");
}

// Test that set the policy to true will allow the API.
IN_PROC_BROWSER_TEST_F(TranslatorAPIPolicyTest, Allow) {
  NavigateToEmptyPage();
  SetTranslatorAPIAllowedPolicy(true);
  TestSimpleTranslationWorks(browser(), "en", "ja");
  TestCanTranslate(browser(), "en", "ja", "readily");
}

// Test that the policy can be dynamically refreshed.
IN_PROC_BROWSER_TEST_F(TranslatorAPIPolicyTest,
                       DynamicRefreshForExistingTranslator) {
  NavigateToEmptyPage();
  // Create a translator.
  ASSERT_EQ(EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), R"(
      (async () => {
        try {
          window._translator = await translation.createTranslator({
              sourceLanguage: 'en',
              targetLanguage: 'ja',
            });
          return 'OK';
        } catch (e) {
          return e.toString();
        }
      })();
      )")
                .ExtractString(),
            "OK");

  // Disallow the API.
  SetTranslatorAPIAllowedPolicy(false);

  // The translator should not be able to translate.
  ASSERT_EQ(EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), R"(
      (async () => {
        try {
          return await window._translator.translate('hello');
        } catch (e) {
          return e.toString();
        }
      })();
      )")
                .ExtractString(),
            "NotReadableError: Unable to translate the given text.");

  // Allow the API.
  SetTranslatorAPIAllowedPolicy(true);

  // The translator should be able to translate.
  ASSERT_EQ(EvalJs(browser()->tab_strip_model()->GetActiveWebContents(), R"(
      (async () => {
        try {
          return await window._translator.translate('hello');
        } catch (e) {
          return e.toString();
        }
      })();
      )")
                .ExtractString(),
            "en to ja: hello");
}

}  // namespace policy
