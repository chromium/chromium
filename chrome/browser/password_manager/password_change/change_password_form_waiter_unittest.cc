// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/mock_password_form_cache.h"
#include "components/password_manager/core/browser/mock_password_manager.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
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

}  // namespace

class PasswordFormWaiterTest : public ChromeRenderViewHostTestHarness {
 public:
  PasswordFormWaiterTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~PasswordFormWaiterTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    OSCryptMocker::SetUp();

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

  password_manager::StubPasswordManagerDriver& driver() { return driver_; }
  password_manager::MockPasswordFormCache& cache() { return mock_cache_; }

  PrefService* prefs() { return profile()->GetPrefs(); }

  password_manager::FakeFormFetcher& form_fetcher() { return form_fetcher_; }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
  MockChromePasswordManagerClient client_;
  scoped_refptr<password_manager::MockPasswordStoreInterface> password_store_ =
      base::MakeRefCounted<password_manager::MockPasswordStoreInterface>();
  password_manager::MockPasswordManager mock_manager_;
  password_manager::MockPasswordFormCache mock_cache_;
  password_manager::FakeFormFetcher form_fetcher_;
  password_manager::StubPasswordManagerDriver driver_;
};

TEST_F(PasswordFormWaiterTest, PasswordChangeFormNotFound) {
  base::MockOnceCallback<void(PasswordFormWaiter::Result)> completion_callback;

  PasswordFormWaiter waiter(web_contents(), client(),
                            completion_callback.Get());

  static_cast<content::WebContentsObserver*>(&waiter)
      ->DocumentOnLoadCompletedInPrimaryMainFrame();
  EXPECT_CALL(completion_callback, Run(PasswordFormWaiter::Result()));
  task_environment()->FastForwardBy(
      PasswordFormWaiter::kChangePasswordFormWaitingTimeout);
}

TEST_F(PasswordFormWaiterTest, NotFoundCallbackInvokedOnlyAfterPageLoaded) {
  base::MockOnceCallback<void(PasswordFormWaiter::Result)> completion_callback;

  PasswordFormWaiter waiter(web_contents(), client(),
                            completion_callback.Get());
  EXPECT_CALL(completion_callback, Run).Times(0);
  task_environment()->FastForwardBy(
      PasswordFormWaiter::kChangePasswordFormWaitingTimeout * 2);

  static_cast<content::WebContentsObserver*>(&waiter)
      ->DocumentOnLoadCompletedInPrimaryMainFrame();
  EXPECT_CALL(completion_callback, Run(PasswordFormWaiter::Result()));
  // Now the timeout starts.
  task_environment()->FastForwardBy(
      PasswordFormWaiter::kChangePasswordFormWaitingTimeout);
}

TEST_F(PasswordFormWaiterTest, NotFoundTimeoutResetOnLoadingEvent) {
  base::MockOnceCallback<void(PasswordFormWaiter::Result)> completion_callback;

  PasswordFormWaiter waiter(web_contents(), client(),
                            completion_callback.Get());
  static_cast<content::WebContentsObserver*>(&waiter)
      ->DocumentOnLoadCompletedInPrimaryMainFrame();

  EXPECT_CALL(completion_callback, Run).Times(0);
  task_environment()->FastForwardBy(
      PasswordFormWaiter::kChangePasswordFormWaitingTimeout / 2);

  // Emulate another loading finished event again.
  static_cast<content::WebContentsObserver*>(&waiter)
      ->DocumentOnLoadCompletedInPrimaryMainFrame();
  // Normally it would trigger timeout, but since another loading happened
  // before it doesn't happen.
  task_environment()->FastForwardBy(
      PasswordFormWaiter::kChangePasswordFormWaitingTimeout / 2);

  EXPECT_CALL(completion_callback, Run(PasswordFormWaiter::Result()));
  // Now finally after waiting 1.5 * kChangePasswordFormWaitingTimeout callback
  // is triggered.
  task_environment()->FastForwardBy(
      PasswordFormWaiter::kChangePasswordFormWaitingTimeout / 2);
  testing::Mock::VerifyAndClearExpectations(&completion_callback);
}

TEST_F(PasswordFormWaiterTest, PasswordChangeFormIdentified) {
  base::MockOnceCallback<void(PasswordFormWaiter::Result)> completion_callback;

  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password_1",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.push_back(CreateTestFormField(
      /*label=*/"Password confirmation:", /*name=*/"new_password_2",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  auto form_manager = CreateFormManager(form);

  PasswordFormWaiter waiter(web_contents(), client(),
                            completion_callback.Get());

  EXPECT_CALL(completion_callback,
              Run(PasswordFormWaiter::Result{.change_password_form_manager =
                                                 form_manager.get()}));
  static_cast<password_manager::PasswordFormManagerObserver*>(&waiter)
      ->OnPasswordFormParsed(form_manager.get());
  testing::Mock::VerifyAndClearExpectations(&completion_callback);
}

TEST_F(PasswordFormWaiterTest, LoginForm) {
  base::MockOnceCallback<void(PasswordFormWaiter::Result)> completion_callback;

  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Username:", /*name=*/"username",
      /*value=*/"", autofill::FormControlType::kInputEmail));
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  auto form_manager = CreateFormManager(form);

  PasswordFormWaiter waiter(web_contents(), client(),
                            completion_callback.Get());

  EXPECT_CALL(completion_callback, Run).Times(0);
  static_cast<password_manager::PasswordFormManagerObserver*>(&waiter)
      ->OnPasswordFormParsed(form_manager.get());
  testing::Mock::VerifyAndClearExpectations(&completion_callback);
}

TEST_F(PasswordFormWaiterTest, SignUpForm) {
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Username:", /*name=*/"username",
      /*value=*/"", autofill::FormControlType::kInputEmail));
  fields.push_back(CreateTestFormField(
      /*label=*/"Create password:", /*name=*/"new_password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.push_back(CreateTestFormField(
      /*label=*/"Confirm password:", /*name=*/"new_password_2",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  autofill::FormData form;
  form.set_renderer_id(autofill::test::MakeFormRendererId());
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  auto form_manager = CreateFormManager(form);

  base::MockOnceCallback<void(PasswordFormWaiter::Result)> completion_callback;
  PasswordFormWaiter waiter(web_contents(), client(),
                            completion_callback.Get());

  EXPECT_CALL(completion_callback, Run).Times(0);
  static_cast<password_manager::PasswordFormManagerObserver*>(&waiter)
      ->OnPasswordFormParsed(form_manager.get());
  testing::Mock::VerifyAndClearExpectations(&completion_callback);
}

TEST_F(PasswordFormWaiterTest, FormlessSettingsPage) {
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Email:", /*name=*/"username",
      /*value=*/"", autofill::FormControlType::kInputEmail));
  fields.push_back(CreateTestFormField(
      /*label=*/"Current password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  autofill::FormData form;
  // Not setting render_id means there is no <form> tag.
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  auto form_manager = CreateFormManager(form);

  base::MockOnceCallback<void(PasswordFormWaiter::Result)> completion_callback;
  PasswordFormWaiter waiter(web_contents(), client(),
                            completion_callback.Get());

  EXPECT_CALL(completion_callback,
              Run(PasswordFormWaiter::Result{.change_password_form_manager =
                                                 form_manager.get()}));
  static_cast<password_manager::PasswordFormManagerObserver*>(&waiter)
      ->OnPasswordFormParsed(form_manager.get());
  testing::Mock::VerifyAndClearExpectations(&completion_callback);
}

TEST_F(PasswordFormWaiterTest, ChangePasswordFormWithHiddenUsername) {
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Username:", /*name=*/"username",
      /*value=*/"", autofill::FormControlType::kInputEmail));
  // Mark username not focusable.
  fields.back().set_is_focusable(false);
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  autofill::FormData form;
  form.set_renderer_id(autofill::test::MakeFormRendererId());
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  auto form_manager = CreateFormManager(form);

  base::MockOnceCallback<void(PasswordFormWaiter::Result)> completion_callback;
  PasswordFormWaiter waiter(web_contents(), client(),
                            completion_callback.Get());

  EXPECT_CALL(completion_callback,
              Run(PasswordFormWaiter::Result{.change_password_form_manager =
                                                 form_manager.get()}));
  static_cast<password_manager::PasswordFormManagerObserver*>(&waiter)
      ->OnPasswordFormParsed(form_manager.get());
  testing::Mock::VerifyAndClearExpectations(&completion_callback);
}

TEST_F(PasswordFormWaiterTest, NewPasswordFieldAlone) {
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password",
      /*value=*/"", autofill::FormControlType::kInputPassword, "new-password"));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  auto form_manager = CreateFormManager(form);

  base::MockOnceCallback<void(PasswordFormWaiter::Result)> completion_callback;
  PasswordFormWaiter waiter(web_contents(), client(),
                            completion_callback.Get());

  EXPECT_CALL(completion_callback,
              Run(PasswordFormWaiter::Result{.change_password_form_manager =
                                                 form_manager.get()}));
  static_cast<password_manager::PasswordFormManagerObserver*>(&waiter)
      ->OnPasswordFormParsed(form_manager.get());
  testing::Mock::VerifyAndClearExpectations(&completion_callback);
}

TEST_F(PasswordFormWaiterTest, ChangePasswordFormWithoutConfirmation) {
  base::MockOnceCallback<void(PasswordFormWaiter::Result)> completion_callback;

  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Old password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  auto form_manager = CreateFormManager(form);

  PasswordFormWaiter waiter(web_contents(), client(),
                            completion_callback.Get());

  EXPECT_CALL(completion_callback,
              Run(PasswordFormWaiter::Result{.change_password_form_manager =
                                                 form_manager.get()}));
  static_cast<password_manager::PasswordFormManagerObserver*>(&waiter)
      ->OnPasswordFormParsed(form_manager.get());
  testing::Mock::VerifyAndClearExpectations(&completion_callback);
}

TEST_F(PasswordFormWaiterTest, ChangePasswordFormWithoutOldPassword) {
  base::MockOnceCallback<void(PasswordFormWaiter::Result)> completion_callback;

  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.push_back(CreateTestFormField(
      /*label=*/"Confirm password:", /*name=*/"new_password_2",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  auto form_manager = CreateFormManager(form);

  PasswordFormWaiter waiter(web_contents(), client(),
                            completion_callback.Get());

  EXPECT_CALL(completion_callback,
              Run(PasswordFormWaiter::Result{.change_password_form_manager =
                                                 form_manager.get()}));
  static_cast<password_manager::PasswordFormManagerObserver*>(&waiter)
      ->OnPasswordFormParsed(form_manager.get());
  testing::Mock::VerifyAndClearExpectations(&completion_callback);
}

TEST_F(PasswordFormWaiterTest, PasswordChangeFormAlreadyParsed) {
  base::test::TestFuture<PasswordFormWaiter::Result> result_future;

  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password_1",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.push_back(CreateTestFormField(
      /*label=*/"Password confirmation:", /*name=*/"new_password_2",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));

  std::vector<std::unique_ptr<password_manager::PasswordFormManager>>
      form_managers;
  form_managers.push_back(CreateFormManager(form));

  EXPECT_CALL(cache(), GetFormManagers)
      .WillOnce(testing::Return(base::span(form_managers)));
  PasswordFormWaiter waiter(web_contents(), client(),
                            result_future.GetCallback());

  EXPECT_EQ(result_future.Get(),
            PasswordFormWaiter::Result{.change_password_form_manager =
                                           form_managers.back().get()});
  result_future.Clear();
}

TEST_F(PasswordFormWaiterTest, LoginFormDetectedButNotifiedOnTimeout) {
  base::MockOnceCallback<void(PasswordFormWaiter::Result)> completion_callback;

  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Username:", /*name=*/"username",
      /*value=*/"", autofill::FormControlType::kInputEmail));
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  auto form_manager = CreateFormManager(form);

  PasswordFormWaiter waiter(web_contents(), client(),
                            completion_callback.Get());
  static_cast<content::WebContentsObserver*>(&waiter)
      ->DocumentOnLoadCompletedInPrimaryMainFrame();

  // Simulate login form detection.
  EXPECT_CALL(completion_callback, Run).Times(0);
  static_cast<password_manager::PasswordFormManagerObserver*>(&waiter)
      ->OnPasswordFormParsed(form_manager.get());

  EXPECT_CALL(completion_callback,
              Run(PasswordFormWaiter::Result{.login_form_manager =
                                                 form_manager.get()}));
  task_environment()->FastForwardBy(
      PasswordFormWaiter::kChangePasswordFormWaitingTimeout);
  testing::Mock::VerifyAndClearExpectations(&completion_callback);
}

TEST_F(PasswordFormWaiterTest, ChangePasswordFormDetectedAfterLoginForm) {
  base::MockOnceCallback<void(PasswordFormWaiter::Result)> completion_callback;

  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Username:", /*name=*/"username",
      /*value=*/"", autofill::FormControlType::kInputEmail));
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  auto form_manager = CreateFormManager(form);

  PasswordFormWaiter waiter(web_contents(), client(),
                            completion_callback.Get());

  // Simulate login form detection.
  EXPECT_CALL(completion_callback, Run).Times(0);
  static_cast<password_manager::PasswordFormManagerObserver*>(&waiter)
      ->OnPasswordFormParsed(form_manager.get());

  std::vector<autofill::FormFieldData> change_form_fields;
  change_form_fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  change_form_fields.push_back(CreateTestFormField(
      /*label=*/"Confirm password:", /*name=*/"new_password_2",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  autofill::FormData change_form;
  change_form.set_url(GURL("https://www.foo.com"));
  change_form.set_fields(std::move(change_form_fields));
  auto change_form_manager = CreateFormManager(change_form);

  EXPECT_CALL(completion_callback,
              Run(PasswordFormWaiter::Result{.change_password_form_manager =
                                                 change_form_manager.get()}));
  static_cast<password_manager::PasswordFormManagerObserver*>(&waiter)
      ->OnPasswordFormParsed(change_form_manager.get());
  testing::Mock::VerifyAndClearExpectations(&completion_callback);
}
