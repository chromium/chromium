// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/android/auto_translate_snackbar_controller.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/mock_translate_ranker.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/translate_errors.h"
#include "components/translate/core/common/translate_metrics.h"
#include "components/translate/core/common/translate_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace translate {
namespace {

using ::testing::_;
using ::testing::Return;

class TestBridge : public AutoTranslateSnackbarController::Bridge {
 public:
  ~TestBridge() override;

  base::WeakPtr<TestBridge> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  std::string GetTargetLanguage() { return target_language_; }

  MOCK_METHOD(bool,
              CreateAutoTranslateSnackbarController,
              (JNIEnv*,
               content::WebContents*,
               AutoTranslateSnackbarController*),
              (override));

  void ShowSnackbar(
      JNIEnv* env,
      base::android::ScopedJavaLocalRef<jstring> target_language) override;
  void WasDismissed() override;
  bool IsSnackbarShowing() override;
  MOCK_METHOD(void, DismissSnackbar, (JNIEnv * env), (override));
  MOCK_METHOD(bool, CanShowSnackbar, (), (override));

 private:
  bool is_showing_;
  std::string target_language_;
  base::WeakPtrFactory<TestBridge> weak_ptr_factory_{this};
};

TestBridge::~TestBridge() = default;

void TestBridge::ShowSnackbar(
    JNIEnv* env,
    base::android::ScopedJavaLocalRef<jstring> target_language) {
  is_showing_ = true;
  target_language_ =
      base::android::ConvertJavaStringToUTF8(env, target_language);
}

void TestBridge::WasDismissed() {
  is_showing_ = false;
}

bool TestBridge::IsSnackbarShowing() {
  return is_showing_;
}

class TestLanguageModel : public language::LanguageModel {
  std::vector<LanguageDetails> GetLanguages() override {
    return {LanguageDetails("en", 1.0)};
  }
};

class TestTranslateDriver : public testing::MockTranslateDriver {
 public:
  ~TestTranslateDriver() override;
  MOCK_METHOD(void, RevertTranslation, (int), (override));
  MOCK_METHOD(bool, IsIncognito, (), (const override));
  MOCK_METHOD(bool, HasCurrentPage, (), (const override));
};

TestTranslateDriver::~TestTranslateDriver() = default;

constexpr const char kInfobarEventHistogram[] =
    "Translate.CompactInfobar.Event";

class AutoTranslateSnackbarControllerTest : public ::testing::Test {
 public:
  AutoTranslateSnackbarControllerTest() {
    // Needed so that driver->ShowTranslteStep will work
    ON_CALL(driver_, IsIncognito()).WillByDefault(Return(false));
    ON_CALL(driver_, HasCurrentPage()).WillByDefault(Return(true));
    driver_.SetLastCommittedURL(GURL("http://www.example.com/"));
  }

 protected:
  void SetUp() override {
    ::testing::Test::SetUp();

    // Setup Mock Translate Manager
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    language::LanguagePrefs::RegisterProfilePrefs(pref_service_->registry());
    TranslatePrefs::RegisterProfilePrefs(pref_service_->registry());

    ranker_ = std::make_unique<testing::MockTranslateRanker>();
    client_ = std::make_unique<testing::MockTranslateClient>(
        &driver_, pref_service_.get());
    // Make Translate Manager
    manager_ = std::make_unique<TranslateManager>(client_.get(), ranker_.get(),
                                                  language_model_.get());
    manager_->GetLanguageState()->set_translation_declined(false);
    TranslateDownloadManager::GetInstance()->set_application_locale("en");

    auto bridge = std::make_unique<TestBridge>();
    bridge_ = bridge->GetWeakPtr();

    auto_snackbar_controller_ =
        std::make_unique<AutoTranslateSnackbarController>(
            /* web_contents= */ nullptr, manager_->GetWeakPtr(),
            std::move(bridge));
  }

  TestTranslateDriver driver_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<testing::MockTranslateClient> client_;
  std::unique_ptr<testing::MockTranslateRanker> ranker_;
  std::unique_ptr<TestLanguageModel> language_model_;
  std::unique_ptr<TranslateManager> manager_;

  base::WeakPtr<TestBridge> bridge_;
  std::unique_ptr<AutoTranslateSnackbarController> auto_snackbar_controller_;
};

TEST_F(AutoTranslateSnackbarControllerTest, CreateAndDismissSnackbarNoAction) {
  JNIEnv* env = base::android::AttachCurrentThread();
  EXPECT_CALL(*bridge_, DismissSnackbar(env));
  EXPECT_CALL(*bridge_, CanShowSnackbar()).WillRepeatedly(Return(true));

  // Show snackbar
  EXPECT_FALSE(bridge_->IsSnackbarShowing());
  auto_snackbar_controller_->ShowSnackbar("tl");
  EXPECT_TRUE(bridge_->IsSnackbarShowing());
  EXPECT_EQ("tl", bridge_->GetTargetLanguage());

  // Dismiss snackbar from Java
  auto_snackbar_controller_->OnDismissNoAction(env);
  EXPECT_FALSE(bridge_->IsSnackbarShowing());
}

TEST_F(AutoTranslateSnackbarControllerTest,
       CreateAndDismissSnackbarWithAction) {
  base::HistogramTester histogram_tester;

  JNIEnv* env = base::android::AttachCurrentThread();
  EXPECT_CALL(*bridge_, DismissSnackbar(env));
  EXPECT_CALL(driver_, RevertTranslation(_));
  EXPECT_CALL(*bridge_, CanShowSnackbar()).WillRepeatedly(Return(true));

  // Show snackbar and fake translation
  EXPECT_FALSE(bridge_->IsSnackbarShowing());
  manager_->PageTranslated("en", "tl", TranslateErrors::NONE);
  auto_snackbar_controller_->ShowSnackbar("tl");
  EXPECT_TRUE(bridge_->IsSnackbarShowing());
  EXPECT_EQ("tl", bridge_->GetTargetLanguage());

  // Click on Undo translation from Java
  auto j_string = base::android::ConvertUTF8ToJavaString(env, "tl");
  auto_snackbar_controller_->OnUndoActionPressed(
      env, base::android::JavaParamRef<jstring>(env, j_string.obj()));
  EXPECT_FALSE(bridge_->IsSnackbarShowing());

  // Check that the INFOBAR_REVERT histogram was recorded
  histogram_tester.ExpectUniqueSample(kInfobarEventHistogram,
                                      InfobarEvent::INFOBAR_REVERT, 1);
}

TEST_F(AutoTranslateSnackbarControllerTest, DismissFromNative) {
  JNIEnv* env = base::android::AttachCurrentThread();
  EXPECT_CALL(*bridge_, DismissSnackbar(env)).Times(2);
  EXPECT_CALL(*bridge_, CanShowSnackbar()).WillRepeatedly(Return(true));

  // Show snackbar
  EXPECT_FALSE(bridge_->IsSnackbarShowing());
  auto_snackbar_controller_->ShowSnackbar("tl");
  EXPECT_TRUE(bridge_->IsSnackbarShowing());
  EXPECT_EQ("tl", bridge_->GetTargetLanguage());

  auto_snackbar_controller_->NativeDismissSnackbar();
}

TEST_F(AutoTranslateSnackbarControllerTest,
       CreateAuotTranslateSnackbarFailsThenSucceeds) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // The first call to CreateAutoTranslateSnakbarController fails.
  EXPECT_CALL(*bridge_, CanShowSnackbar()).WillRepeatedly(Return(false));
  EXPECT_CALL(*bridge_, CreateAutoTranslateSnackbarController(env, _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(*bridge_, DismissSnackbar(env));

  auto_snackbar_controller_->ShowSnackbar("tl");

  // The Snackbar should not be showing if the controller was not created.
  EXPECT_FALSE(auto_snackbar_controller_->IsShowing());

  // The second call to CreateAutoTranslateSnakbarController succeeds.
  EXPECT_CALL(*bridge_, CreateAutoTranslateSnackbarController(env, _, _))
      .WillOnce(Return(true));

  auto_snackbar_controller_->ShowSnackbar("tl");

  // The Snackbar should now be showing since the controller was succefully
  // created.
  EXPECT_TRUE(auto_snackbar_controller_->IsShowing());
}

}  // namespace
}  // namespace translate
