// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
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

class FakeChromePasswordManagerClient : public ChromePasswordManagerClient {
 public:
  static void CreateForWebContents(content::WebContents* contents) {
    auto* client = new FakeChromePasswordManagerClient(contents);
    contents->SetUserData(UserDataKey(), base::WrapUnique(client));
  }

  password_manager::WebAuthnCredentialsDelegate*
  GetWebAuthnCredentialsDelegateForDriver(
      password_manager::PasswordManagerDriver*) override {
    return nullptr;
  }

 private:
  explicit FakeChromePasswordManagerClient(content::WebContents* web_contents)
      : ChromePasswordManagerClient(web_contents) {}
};

}  // namespace

class ChangePasswordFormWaiterTest : public ChromeRenderViewHostTestHarness {
 public:
  ChangePasswordFormWaiterTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ChangePasswordFormWaiterTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    OSCryptMocker::SetUp();

    ProfilePasswordStoreFactory::GetInstance()->SetTestingFactory(
        GetBrowserContext(),
        base::BindRepeating(&password_manager::BuildPasswordStoreInterface<
                            content::BrowserContext,
                            password_manager::MockPasswordStoreInterface>));

    // `ChromePasswordManagerClient` observes `AutofillManager`s, so
    // `ChromeAutofillClient` needs to be set up, too.
    autofill::ChromeAutofillClient::CreateForWebContents(web_contents());
    FakeChromePasswordManagerClient::CreateForWebContents(web_contents());
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

  ChromePasswordManagerClient* client() {
    return ChromePasswordManagerClient::FromWebContents(web_contents());
  }

  password_manager::StubPasswordManagerDriver& driver() { return driver_; }

  PrefService* prefs() { return profile()->GetPrefs(); }

  password_manager::FakeFormFetcher& form_fetcher() { return form_fetcher_; }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
  password_manager::FakeFormFetcher form_fetcher_;
  password_manager::StubPasswordManagerDriver driver_;
};

TEST_F(ChangePasswordFormWaiterTest, PasswordChangeFormNotFound) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;

  ChangePasswordFormWaiter waiter(web_contents(), completion_callback.Get());

  static_cast<content::WebContentsObserver*>(&waiter)
      ->DocumentOnLoadCompletedInPrimaryMainFrame();
  EXPECT_CALL(completion_callback, Run(nullptr));
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);
}

TEST_F(ChangePasswordFormWaiterTest,
       NotFoundCallbackInvokedOnlyAfterPageLoaded) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;

  ChangePasswordFormWaiter waiter(web_contents(), completion_callback.Get());
  EXPECT_CALL(completion_callback, Run).Times(0);
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout * 2);

  static_cast<content::WebContentsObserver*>(&waiter)
      ->DocumentOnLoadCompletedInPrimaryMainFrame();
  EXPECT_CALL(completion_callback, Run(nullptr));
  // Now the timeout starts.
  task_environment()->FastForwardBy(
      ChangePasswordFormWaiter::kChangePasswordFormWaitingTimeout);
}

TEST_F(ChangePasswordFormWaiterTest, PasswordChangeFormIdentified) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;

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

  ChangePasswordFormWaiter waiter(web_contents(), completion_callback.Get());

  EXPECT_CALL(completion_callback, Run(form_manager.get()));
  static_cast<password_manager::PasswordFormManagerObserver*>(&waiter)
      ->OnPasswordFormParsed(form_manager.get());
}

TEST_F(ChangePasswordFormWaiterTest,
       PasswordChangeFormNotFoundWithoutConfirmationField) {
  base::MockOnceCallback<void(password_manager::PasswordFormManager*)>
      completion_callback;

  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  fields.push_back(CreateTestFormField(
      /*label=*/"New password:", /*name=*/"new_password_1",
      /*value=*/"", autofill::FormControlType::kInputPassword));
  autofill::FormData form;
  form.set_url(GURL("https://www.foo.com"));
  form.set_fields(std::move(fields));
  auto form_manager = CreateFormManager(form);

  ChangePasswordFormWaiter waiter(web_contents(), completion_callback.Get());

  EXPECT_CALL(completion_callback, Run).Times(0);
  static_cast<password_manager::PasswordFormManagerObserver*>(&waiter)
      ->OnPasswordFormParsed(form_manager.get());
}
