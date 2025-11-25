// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"

#include "base/functional/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/ml_model/field_classification_model_handler.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/delivery/test_optimization_guide_model_provider.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/mock_password_form_cache.h"
#include "components/password_manager/core/browser/mock_password_manager.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using autofill::test::CreateTestFormField;
using testing::Return;

class MockChromePasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(password_manager::PasswordStoreInterface*,
              GetProfilePasswordStore,
              (),
              (override, const));
  MOCK_METHOD(password_manager::PasswordManagerInterface*,
              GetPasswordManager,
              (),
              (override, const));
};

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MOCK_METHOD(void,
              CheckViewAreaVisible,
              (autofill::FieldRendererId, base::OnceCallback<void(bool)>),
              (override));
};

class FakeFieldClassificationModelHandler
    : public autofill::FieldClassificationModelHandler {
 public:
  FakeFieldClassificationModelHandler(
      optimization_guide::TestOptimizationGuideModelProvider* model_provider)
      : FieldClassificationModelHandler(
            model_provider,
            optimization_guide::proto::OptimizationTarget::
                OPTIMIZATION_TARGET_PASSWORD_MANAGER_FORM_CLASSIFICATION) {}
  ~FakeFieldClassificationModelHandler() override = default;

  void NotifyAboutModelChange() { model_change_callback_list_.Notify(); }

  void SetModelAvailability(bool available) { is_model_available_ = available; }

  // autofill::FieldClassificationModelHandler
  bool ModelAvailable() const override { return is_model_available_; }
  base::CallbackListSubscription RegisterModelChangeCallback(
      ModelChangeCallbackList::CallbackType callback) override {
    return model_change_callback_list_.Add(std::move(callback));
  }

 private:
  bool is_model_available_ = false;
  ModelChangeCallbackList model_change_callback_list_;
};

autofill::FormFieldData CreateNonFocusableTestFormField(
    std::string label,
    std::string name,
    std::string value,
    autofill::FormControlType type) {
  auto field = CreateTestFormField(label, name, value, type);
  field.set_is_focusable(false);
  return field;
}

}  // namespace

class ChangePasswordFormWaiterTest : public ChromeRenderViewHostTestHarness,
                                     public testing::WithParamInterface<bool> {
 public:
  ChangePasswordFormWaiterTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeatureStates(
        {{password_manager::features::
              kCheckVisibilityInChangePasswordFormWaiter,
          GetParam()}});
  }
  ~ChangePasswordFormWaiterTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    OSCryptMocker::SetUp();

    model_provider_ = std::make_unique<
        optimization_guide::TestOptimizationGuideModelProvider>();

    autofill_client()->set_password_ml_prediction_model_handler(
        std::make_unique<FakeFieldClassificationModelHandler>(
            model_provider_.get()));

    ON_CALL(client_, GetProfilePasswordStore)
        .WillByDefault(Return(password_store_.get()));
    ON_CALL(client_, GetPasswordManager).WillByDefault(Return(&mock_manager_));
    ON_CALL(mock_manager_, GetPasswordFormCache)
        .WillByDefault(Return(&mock_cache_));
  }

  std::unique_ptr<password_manager::PasswordFormManager> CreateFormManager(
      const autofill::FormData& form_data) {
    auto form_manager = std::make_unique<password_manager::PasswordFormManager>(
        client(), driver().AsWeakPtr(), form_data, &form_fetcher(),
        std::make_unique<password_manager::PasswordSaveManagerImpl>(client()),
        /*metrics_recorder=*/nullptr);
    // Force form parsing, otherwise there will be no parsed observed form.
    form_fetcher_.NotifyFetchCompleted();
    static_cast<password_manager::PasswordFormPredictionWaiter::Client*>(
        form_manager.get())
        ->OnWaitCompleted();
    return form_manager;
  }

  password_manager::PasswordManagerClient* client() { return &client_; }

  MockPasswordManagerDriver& driver() { return driver_; }
  password_manager::MockPasswordFormCache& cache() { return mock_cache_; }

  PrefService* prefs() { return profile()->GetPrefs(); }

  password_manager::FakeFormFetcher& form_fetcher() { return form_fetcher_; }

  autofill::TestContentAutofillClient* autofill_client() {
    return autofill_client_injector_[web_contents()];
  }

  FakeFieldClassificationModelHandler* model_handler() {
    return static_cast<FakeFieldClassificationModelHandler*>(
        autofill_client()->GetPasswordManagerFieldClassificationModelHandler());
  }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
  base::test::ScopedFeatureList scoped_feature_list_;
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      autofill_client_injector_;
  MockChromePasswordManagerClient client_;
  scoped_refptr<password_manager::MockPasswordStoreInterface> password_store_ =
      base::MakeRefCounted<password_manager::MockPasswordStoreInterface>();
  password_manager::MockPasswordManager mock_manager_;
  password_manager::MockPasswordFormCache mock_cache_;
  password_manager::FakeFormFetcher form_fetcher_;
  MockPasswordManagerDriver driver_;
  std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>
      model_provider_;
};

TEST_P(ChangePasswordFormWaiterTest, PasswordChangeFormNotFound) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockOnceClosure timeout_callback;

  auto waiter = ChangePasswordFormWaiter::Builder(web_contents(), client(),
                                                  completion_callback.Get())
                    .SetTimeoutCallback(timeout_callback.Get())
                    .Build();

  static_cast<content::WebContentsObserver*>(waiter.get())->DidStopLoading();
  EXPECT_CALL(completion_callback, Run).Times(0);
  EXPECT_CALL(timeout_callback, Run());
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);
}

TEST_P(ChangePasswordFormWaiterTest,
       NotFoundCallbackInvokedOnlyAfterPageLoaded) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;

  auto waiter = ChangePasswordFormWaiter::Builder(web_contents(), client(),
                                                  completion_callback.Get())
                    .SetTimeoutCallback(base::DoNothing())
                    .Build();
  EXPECT_CALL(completion_callback, Run).Times(0);
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout * 2);

  static_cast<content::WebContentsObserver*>(waiter.get())->DidStopLoading();
  EXPECT_CALL(completion_callback, Run).Times(0);
  // Now the timeout starts.
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);
}

TEST_P(ChangePasswordFormWaiterTest, NotFoundTimeoutResetOnLoadingEvent) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockOnceClosure timeout_callback;

  auto waiter = ChangePasswordFormWaiter::Builder(web_contents(), client(),
                                                  completion_callback.Get())
                    .SetTimeoutCallback(timeout_callback.Get())
                    .Build();
  static_cast<content::WebContentsObserver*>(waiter.get())->DidStopLoading();

  EXPECT_CALL(timeout_callback, Run).Times(0);
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout / 2);

  // Emulate another loading finished event again.
  static_cast<content::WebContentsObserver*>(waiter.get())->DidStopLoading();
  // Normally it would trigger timeout, but since another loading happened
  // before it doesn't happen.
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout / 2);

  EXPECT_CALL(timeout_callback, Run());
  // Now finally after waiting 1.5 * kChangePasswordFormWaitingTimeout callback
  // is triggered.
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout / 2);
}

TEST_P(ChangePasswordFormWaiterTest, NewLoadingStopsTheCurrentTimer) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockOnceClosure timeout_callback;

  auto waiter = ChangePasswordFormWaiter::Builder(web_contents(), client(),
                                                  completion_callback.Get())
                    .SetTimeoutCallback(timeout_callback.Get())
                    .Build();
  static_cast<content::WebContentsObserver*>(waiter.get())->DidStopLoading();

  EXPECT_CALL(timeout_callback, Run).Times(0);
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout / 2);

  // Emulate another loading finished event again.
  static_cast<content::WebContentsObserver*>(waiter.get())->DidStartLoading();
  // Normally it would trigger timeout, but since another loading happened
  // before it doesn't happen.
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);
}

TEST_P(ChangePasswordFormWaiterTest, PasswordChangeFormIdentified) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;

  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(1));
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password_1",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(2));
  fields.push_back(CreateTestFormField(
      /*label=*/"Password confirmation:", /*name=*/"new_password_2",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(3));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  std::vector<std::unique_ptr<password_manager::PasswordFormManager>>
      form_managers;
  form_managers.push_back(CreateFormManager(form));

  auto waiter = ChangePasswordFormWaiter::Builder(web_contents(), client(),
                                                  completion_callback.Get())
                    .Build();
  if (GetParam()) {
    EXPECT_CALL(cache(), GetFormManagers)
        .WillOnce(testing::Return(base::span(form_managers)));
    EXPECT_CALL(driver(),
                CheckViewAreaVisible(autofill::FieldRendererId(2), testing::_))
        .WillOnce(base::test::RunOnceCallback<1>(true));
  }

  EXPECT_CALL(completion_callback, Run(form_managers.back().get()));
  static_cast<password_manager::PasswordFormManagerObserver*>(waiter.get())
      ->OnPasswordFormParsed(form_managers.back().get());
}

TEST_P(ChangePasswordFormWaiterTest,
       PasswordChangeFormIdentified_HiddenFormIgnored) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;

  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateNonFocusableTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(1));
  fields.push_back(CreateNonFocusableTestFormField(
      /*label=*/"New password:", /*name=*/"new_password_1",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(2));
  fields.push_back(CreateNonFocusableTestFormField(
      /*label=*/"Password confirmation:", /*name=*/"new_password_2",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(3));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  std::vector<std::unique_ptr<password_manager::PasswordFormManager>>
      form_managers;
  form_managers.push_back(CreateFormManager(form));

  auto waiter = ChangePasswordFormWaiter::Builder(web_contents(), client(),
                                                  completion_callback.Get())
                    .IgnoreHiddenForms()
                    .Build();

  if (GetParam()) {
    EXPECT_CALL(driver(),
                CheckViewAreaVisible(autofill::FieldRendererId(2), testing::_))
        .WillOnce(base::test::RunOnceCallback<1>(false));
  }

  EXPECT_CALL(completion_callback, Run).Times(0);
  static_cast<password_manager::PasswordFormManagerObserver*>(waiter.get())
      ->OnPasswordFormParsed(form_managers.back().get());
}

TEST_P(ChangePasswordFormWaiterTest,
       PasswordChangeFormIdentified_HiddenFormNotIgnored) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;

  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateNonFocusableTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(1));
  fields.push_back(CreateNonFocusableTestFormField(
      /*label=*/"New password:", /*name=*/"new_password_1",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(2));
  fields.push_back(CreateNonFocusableTestFormField(
      /*label=*/"Password confirmation:", /*name=*/"new_password_2",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(3));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  std::vector<std::unique_ptr<password_manager::PasswordFormManager>>
      form_managers;
  form_managers.push_back(CreateFormManager(form));

  auto waiter = ChangePasswordFormWaiter::Builder(web_contents(), client(),
                                                  completion_callback.Get())
                    .Build();

  if (GetParam()) {
    EXPECT_CALL(driver(),
                CheckViewAreaVisible(autofill::FieldRendererId(2), testing::_))
        .WillOnce(base::test::RunOnceCallback<1>(false));
    // With kCheckVisibilityInChangePasswordFormWaiter enabled hidden forms are
    // always ignored.
    EXPECT_CALL(completion_callback, Run).Times(0);
  } else {
    EXPECT_CALL(completion_callback, Run(form_managers.back().get()));
  }

  static_cast<password_manager::PasswordFormManagerObserver*>(waiter.get())
      ->OnPasswordFormParsed(form_managers.back().get());
}

TEST_P(ChangePasswordFormWaiterTest, IgnoredChangePasswordForm) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockOnceClosure timeout_callback;

  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(1));
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password_1",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(2));
  fields.push_back(CreateTestFormField(
      /*label=*/"Password confirmation:", /*name=*/"new_password_2",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(3));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  auto form_manager = CreateFormManager(form);

  const auto* parsed_form = form_manager->GetParsedObservedForm();
  ASSERT_TRUE(parsed_form);
  autofill::FieldRendererId new_password_renderer_id =
      parsed_form->new_password_element_renderer_id;
  ASSERT_EQ(new_password_renderer_id, autofill::FieldRendererId(2));

  auto waiter = ChangePasswordFormWaiter::Builder(web_contents(), client(),
                                                  completion_callback.Get())
                    .SetFieldsToIgnore({new_password_renderer_id})
                    .SetTimeoutCallback(timeout_callback.Get())
                    .Build();

  EXPECT_CALL(driver(), CheckViewAreaVisible).Times(0);

  // The form is ignored because its new password field is in
  // `fields_to_ignore`.
  EXPECT_CALL(completion_callback, Run).Times(0);
  static_cast<password_manager::PasswordFormManagerObserver*>(waiter.get())
      ->OnPasswordFormParsed(form_manager.get());
  testing::Mock::VerifyAndClearExpectations(&completion_callback);

  static_cast<content::WebContentsObserver*>(waiter.get())->DidStopLoading();
  EXPECT_CALL(timeout_callback, Run());
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);
}

TEST_P(ChangePasswordFormWaiterTest, FormlessSettingsPage) {
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Email:", /*name=*/"username",
      /*value=*/"username@example.com",
      autofill::FormControlType::kInputEmail));
  fields.back().set_renderer_id(autofill::FieldRendererId(1));
  fields.push_back(CreateTestFormField(
      /*label=*/"Current password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(2));
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(3));
  autofill::FormData form;
  // Not setting render_id means there is no <form> tag.
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  std::vector<std::unique_ptr<password_manager::PasswordFormManager>>
      form_managers;
  form_managers.push_back(CreateFormManager(form));

  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  auto waiter = ChangePasswordFormWaiter::Builder(web_contents(), client(),
                                                  completion_callback.Get())
                    .Build();

  if (GetParam()) {
    EXPECT_CALL(cache(), GetFormManagers)
        .WillOnce(testing::Return(base::span(form_managers)));
    EXPECT_CALL(driver(),
                CheckViewAreaVisible(autofill::FieldRendererId(3), testing::_))
        .WillOnce(base::test::RunOnceCallback<1>(true));
  }

  EXPECT_CALL(completion_callback, Run(form_managers.back().get()));
  static_cast<password_manager::PasswordFormManagerObserver*>(waiter.get())
      ->OnPasswordFormParsed(form_managers.back().get());
}

TEST_P(ChangePasswordFormWaiterTest, ChangePasswordFormWithHiddenUsername) {
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Username:", /*name=*/"username",
      /*value=*/"username@example.com",
      autofill::FormControlType::kInputEmail));
  // Mark username not focusable.
  fields.back().set_is_focusable(false);
  fields.back().set_renderer_id(autofill::FieldRendererId(1));
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(2));
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(3));
  autofill::FormData form;
  form.set_renderer_id(autofill::test::MakeFormRendererId());
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  std::vector<std::unique_ptr<password_manager::PasswordFormManager>>
      form_managers;
  form_managers.push_back(CreateFormManager(form));

  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  auto waiter = ChangePasswordFormWaiter::Builder(web_contents(), client(),
                                                  completion_callback.Get())
                    .Build();

  if (GetParam()) {
    EXPECT_CALL(cache(), GetFormManagers)
        .WillOnce(testing::Return(base::span(form_managers)));
    EXPECT_CALL(driver(),
                CheckViewAreaVisible(autofill::FieldRendererId(3), testing::_))
        .WillOnce(base::test::RunOnceCallback<1>(true));
  }

  EXPECT_CALL(completion_callback, Run(form_managers.back().get()));
  static_cast<password_manager::PasswordFormManagerObserver*>(waiter.get())
      ->OnPasswordFormParsed(form_managers.back().get());
}

TEST_P(ChangePasswordFormWaiterTest, SignUpForm) {
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Username:", /*name=*/"username",
      /*value=*/"", autofill::FormControlType::kInputEmail));
  fields.back().set_renderer_id(autofill::FieldRendererId(1));
  fields.push_back(CreateTestFormField(
      /*label=*/"Create password:", /*name=*/"new_password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(2));
  autofill::FormData form;
  form.set_renderer_id(autofill::test::MakeFormRendererId());
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  auto form_manager = CreateFormManager(form);

  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  auto waiter = ChangePasswordFormWaiter::Builder(web_contents(), client(),
                                                  completion_callback.Get())
                    .Build();

  EXPECT_CALL(completion_callback, Run).Times(0);
  static_cast<password_manager::PasswordFormManagerObserver*>(waiter.get())
      ->OnPasswordFormParsed(form_manager.get());
}

TEST_P(ChangePasswordFormWaiterTest, NewPasswordFieldAlone) {
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password",
      /*value=*/"", autofill::FormControlType::kInputPassword, "new-password"));
  fields.back().set_renderer_id(autofill::FieldRendererId(1));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  std::vector<std::unique_ptr<password_manager::PasswordFormManager>>
      form_managers;
  form_managers.push_back(CreateFormManager(form));

  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  auto waiter = ChangePasswordFormWaiter::Builder(web_contents(), client(),
                                                  completion_callback.Get())
                    .Build();

  if (GetParam()) {
    EXPECT_CALL(cache(), GetFormManagers)
        .WillOnce(testing::Return(base::span(form_managers)));
    EXPECT_CALL(driver(),
                CheckViewAreaVisible(autofill::FieldRendererId(1), testing::_))
        .WillOnce(base::test::RunOnceCallback<1>(true));
  }

  EXPECT_CALL(completion_callback, Run(form_managers.back().get()));
  static_cast<password_manager::PasswordFormManagerObserver*>(waiter.get())
      ->OnPasswordFormParsed(form_managers.back().get());
}

TEST_P(ChangePasswordFormWaiterTest, ChangePasswordFormWithoutConfirmation) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;

  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Old password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(1));
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(2));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  std::vector<std::unique_ptr<password_manager::PasswordFormManager>>
      form_managers;
  form_managers.push_back(CreateFormManager(form));

  auto waiter = ChangePasswordFormWaiter::Builder(web_contents(), client(),
                                                  completion_callback.Get())
                    .Build();

  if (GetParam()) {
    EXPECT_CALL(cache(), GetFormManagers)
        .WillOnce(testing::Return(base::span(form_managers)));
    EXPECT_CALL(driver(),
                CheckViewAreaVisible(autofill::FieldRendererId(2), testing::_))
        .WillOnce(base::test::RunOnceCallback<1>(true));
  }

  EXPECT_CALL(completion_callback, Run(form_managers.back().get()));
  static_cast<password_manager::PasswordFormManagerObserver*>(waiter.get())
      ->OnPasswordFormParsed(form_managers.back().get());
}

TEST_P(ChangePasswordFormWaiterTest, ChangePasswordFormWithoutOldPassword) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;

  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(1));
  fields.push_back(CreateTestFormField(
      /*label=*/"Confirm password:", /*name=*/"new_password_2",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(2));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  std::vector<std::unique_ptr<password_manager::PasswordFormManager>>
      form_managers;
  form_managers.push_back(CreateFormManager(form));

  auto waiter = ChangePasswordFormWaiter::Builder(web_contents(), client(),
                                                  completion_callback.Get())
                    .Build();

  if (GetParam()) {
    EXPECT_CALL(cache(), GetFormManagers)
        .WillOnce(testing::Return(base::span(form_managers)));
    EXPECT_CALL(driver(),
                CheckViewAreaVisible(autofill::FieldRendererId(2), testing::_))
        .WillOnce(base::test::RunOnceCallback<1>(true));
  }

  EXPECT_CALL(completion_callback, Run(form_managers.back().get()));
  static_cast<password_manager::PasswordFormManagerObserver*>(waiter.get())
      ->OnPasswordFormParsed(form_managers.back().get());
}

TEST_P(ChangePasswordFormWaiterTest, PasswordChangeFormAlreadyParsed) {
  base::test::TestFuture<password_manager::PasswordFormManager*> result_future;

  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(1));
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password_1",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(2));
  fields.push_back(CreateTestFormField(
      /*label=*/"Password confirmation:", /*name=*/"new_password_2",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(3));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));

  std::vector<std::unique_ptr<password_manager::PasswordFormManager>>
      form_managers;
  form_managers.push_back(CreateFormManager(form));

  EXPECT_CALL(cache(), GetFormManagers)
      .WillRepeatedly(testing::Return(base::span(form_managers)));
  auto waiter = ChangePasswordFormWaiter::Builder(web_contents(), client(),
                                                  result_future.GetCallback())
                    .Build();
  if (GetParam()) {
    EXPECT_CALL(driver(),
                CheckViewAreaVisible(autofill::FieldRendererId(2), testing::_))
        .WillOnce(base::test::RunOnceCallback<1>(true));
  }

  EXPECT_EQ(result_future.Get(), form_managers.back().get());
}

TEST_P(ChangePasswordFormWaiterTest, FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {password_manager::features::kPasswordFormClientsideClassifier},
      {password_manager::features::kDownloadModelForPasswordChange});

  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  auto waiter = ChangePasswordFormWaiter::Builder(web_contents(), client(),
                                                  completion_callback.Get())
                    .Build();

  // The rest of the test is similar to PasswordChangeFormIdentified.
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(1));
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password_1",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(2));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  std::vector<std::unique_ptr<password_manager::PasswordFormManager>>
      form_managers;
  form_managers.push_back(CreateFormManager(form));

  if (GetParam()) {
    EXPECT_CALL(cache(), GetFormManagers)
        .WillOnce(testing::Return(base::span(form_managers)));
    EXPECT_CALL(driver(),
                CheckViewAreaVisible(autofill::FieldRendererId(2), testing::_))
        .WillOnce(base::test::RunOnceCallback<1>(true));
  }

  EXPECT_CALL(completion_callback, Run(form_managers.back().get()));
  static_cast<password_manager::PasswordFormManagerObserver*>(waiter.get())
      ->OnPasswordFormParsed(form_managers.back().get());
}

TEST_P(ChangePasswordFormWaiterTest, FeatureEnabled_ModelAvailable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {password_manager::features::kPasswordFormClientsideClassifier,
       password_manager::features::kDownloadModelForPasswordChange},
      {});

  model_handler()->SetModelAvailability(/*available=*/true);

  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  auto waiter = ChangePasswordFormWaiter::Builder(web_contents(), client(),
                                                  completion_callback.Get())
                    .Build();

  // The rest of the test is similar to PasswordChangeFormIdentified.
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(1));
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password_1",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(2));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  std::vector<std::unique_ptr<password_manager::PasswordFormManager>>
      form_managers;
  form_managers.push_back(CreateFormManager(form));

  if (GetParam()) {
    EXPECT_CALL(cache(), GetFormManagers)
        .WillOnce(testing::Return(base::span(form_managers)));
    EXPECT_CALL(driver(),
                CheckViewAreaVisible(autofill::FieldRendererId(2), testing::_))
        .WillOnce(base::test::RunOnceCallback<1>(true));
  }

  EXPECT_CALL(completion_callback, Run(form_managers.back().get()));
  static_cast<password_manager::PasswordFormManagerObserver*>(waiter.get())
      ->OnPasswordFormParsed(form_managers.back().get());
}

TEST_P(ChangePasswordFormWaiterTest, FeatureEnabled_ModelBecomesAvailable) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {password_manager::features::kPasswordFormClientsideClassifier,
       password_manager::features::kDownloadModelForPasswordChange},
      {});
  model_handler()->SetModelAvailability(/*available=*/false);

  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(1));
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password_1",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(2));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  std::vector<std::unique_ptr<password_manager::PasswordFormManager>>
      form_managers;
  form_managers.push_back(CreateFormManager(form));

  EXPECT_CALL(cache(), GetFormManagers)
      .WillRepeatedly(testing::Return(base::span(form_managers)));

  base::test::TestFuture<password_manager::PasswordFormManager*> result_future;
  auto waiter = ChangePasswordFormWaiter::Builder(web_contents(), client(),
                                                  result_future.GetCallback())
                    .Build();

  // Model is not available yet, so the callback should not be called.
  EXPECT_FALSE(result_future.IsReady());

  if (GetParam()) {
    EXPECT_CALL(driver(),
                CheckViewAreaVisible(autofill::FieldRendererId(2), testing::_))
        .WillOnce(base::test::RunOnceCallback<1>(true));
  }
  // Simulate the model becoming available.
  model_handler()->NotifyAboutModelChange();

  EXPECT_EQ(result_future.Get(), form_managers.back().get());
}

TEST_P(ChangePasswordFormWaiterTest, FeatureEnabled_ModelNotAvailable_Timeout) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {password_manager::features::kPasswordFormClientsideClassifier,
       password_manager::features::kDownloadModelForPasswordChange},
      {});

  model_handler()->SetModelAvailability(/*available=*/false);

  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;
  base::MockOnceClosure timeout_callback;
  auto waiter = ChangePasswordFormWaiter::Builder(web_contents(), client(),
                                                  completion_callback.Get())
                    .SetTimeoutCallback(timeout_callback.Get())
                    .Build();

  // Model is not available yet, so the callback should not be called.
  EXPECT_CALL(completion_callback, Run).Times(0);
  EXPECT_CALL(timeout_callback, Run).Times(0);

  // Timeout should not be triggered even if the model is not available.
  static_cast<content::WebContentsObserver*>(waiter.get())->DidStopLoading();
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout * 2);

  // Simulate the model becoming available.
  model_handler()->NotifyAboutModelChange();

  static_cast<content::WebContentsObserver*>(waiter.get())->DidStopLoading();
  EXPECT_CALL(timeout_callback, Run());
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);
}

INSTANTIATE_TEST_SUITE_P(, ChangePasswordFormWaiterTest, testing::Bool());
