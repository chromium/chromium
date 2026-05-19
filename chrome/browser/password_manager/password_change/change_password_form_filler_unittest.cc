// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/change_password_form_filler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/factories/profile_password_store_factory.h"
#include "chrome/browser/password_manager/password_change/change_password_form_waiter.h"
#include "chrome/browser/password_manager/password_change/model_quality_logs_uploader.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/mock_password_form_cache.h"
#include "components/password_manager/core/browser/mock_password_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using SubmissionError = ChangePasswordFormFiller::SubmissionError;
using FillingResult = ChangePasswordFormFiller::FillingResult;

const std::u16string kUsername = u"user";
const std::u16string kOldPassword = u"qwerty123";
const std::u16string kNewPassword = u"cE1L45Vgxyzlu8";
const char kUrlString[] = "https://www.foo.com/";
const int password_renderer_id = 1;
const int new_password_renderer_id = 2;

template <typename... Args>
autofill::FormFieldData CreateTestFormField(Args&&... args) {
  auto field = autofill::test::CreateTestFormField(std::forward<Args>(args)...);
  field.set_is_enabled(true);
  return field;
}

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

class MockStubPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MOCK_METHOD(
      void,
      FillChangePasswordForm,
      (autofill::FieldRendererId,
       autofill::FieldRendererId,
       autofill::FieldRendererId,
       const std::u16string&,
       const std::u16string&,
       base::OnceCallback<void(const std::optional<autofill::FormData>&)>),
      (override));
  MOCK_METHOD(void,
              CheckViewAreaVisible,
              (autofill::FieldRendererId, base::OnceCallback<void(bool)>),
              (override));
};

autofill::FormData CreateTestPasswordFormData(
    const std::string& old_password,
    const std::string& new_password,
    int password_id = password_renderer_id,
    int new_password_id = new_password_renderer_id) {
  std::vector<autofill::FormFieldData> fields;
  fields.push_back(CreateTestFormField(
      /*label=*/"Password:", /*name=*/"password",
      /*value=*/old_password, autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(password_id));
  fields.push_back(CreateTestFormField(
      /*label=*/"New Password:", /*name=*/"new-password",
      /*value=*/new_password, autofill::FormControlType::kInputPassword));
  fields.back().set_renderer_id(autofill::FieldRendererId(new_password_id));
  autofill::FormData form;
  form.set_url(GURL(kUrlString));
  form.set_fields(std::move(fields));
  return form;
}

autofill::FormData CreateEmptyTestPasswordFormData() {
  return CreateTestPasswordFormData("", "");
}

autofill::FormData CreateFilledTestPasswordFormData() {
  return CreateTestPasswordFormData(base::UTF16ToUTF8(kOldPassword),
                                    base::UTF16ToUTF8(kNewPassword));
}

}  // namespace

class ChangePasswordFormFillerTest : public ChromeRenderViewHostTestHarness {
 public:
  ChangePasswordFormFillerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~ChangePasswordFormFillerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ON_CALL(client_, GetProfilePasswordStore)
        .WillByDefault(testing::Return(password_store_.get()));
    ON_CALL(client_, GetPasswordManager)
        .WillByDefault(testing::Return(&mock_manager_));
    ON_CALL(mock_manager_, GetPasswordFormCache)
        .WillByDefault(testing::Return(&mock_cache_));
    ON_CALL(driver_, CheckViewAreaVisible)
        .WillByDefault(base::test::RunOnceCallback<1>(true));

    logs_uploader_ =
        std::make_unique<ModelQualityLogsUploader>(web_contents(), GURL());

    existing_credential_.username_value = kUsername;
    existing_credential_.password_value = kOldPassword;
    existing_credential_.url = url();
    existing_credential_.match_type =
        password_manager::PasswordForm::MatchType::kExact;
    existing_credential_.in_store =
        password_manager::PasswordForm::Store::kProfileStore;
    existing_credential_.scheme = password_manager::PasswordForm::Scheme::kHtml;
  }

  void TearDown() override {
    logs_uploader_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  password_manager::PasswordFormManager* CreateFormManagerFromFormData(
      const autofill::FormData& form_data,
      const std::vector<password_manager::PasswordForm>& credentials_to_seed) {
    auto form_manager = std::make_unique<password_manager::PasswordFormManager>(
        client(), driver().AsWeakPtr(), form_data, &form_fetcher(),
        std::make_unique<password_manager::PasswordSaveManagerImpl>(client()),
        /*metrics_recorder=*/nullptr);
    form_fetcher_.SetBestMatches(credentials_to_seed);
    form_fetcher_.SetNonFederated(credentials_to_seed);
    form_fetcher_.NotifyFetchCompleted();
    static_cast<password_manager::PasswordFormPredictionWaiter::Client*>(
        form_manager.get())
        ->OnWaitCompleted();
    managers_.push_back(std::move(form_manager));
    EXPECT_CALL(mock_cache_, GetFormManagers)
        .WillRepeatedly(testing::Return(base::span(managers_)));
    return managers_.back().get();
  }

  password_manager::PasswordFormManager* CreateFormManager(
      const std::vector<password_manager::PasswordForm>& credentials_to_seed) {
    return CreateFormManagerFromFormData(CreateEmptyTestPasswordFormData(),
                                         credentials_to_seed);
  }

  std::unique_ptr<ChangePasswordFormFiller> CreateFiller() {
    return std::make_unique<ChangePasswordFormFiller>(web_contents(), client(),
                                                      logs_uploader_.get());
  }

  void CompleteFormFilling(password_manager::PasswordFormManager* manager,
                           ChangePasswordFormFiller* filler,
                           std::optional<autofill::FormData> result,
                           base::OnceCallback<void(FillingResult)> callback) {
    base::RunLoop run_loop;
    EXPECT_CALL(driver(), FillChangePasswordForm)
        .WillOnce(DoAll(Invoke(&run_loop, &base::RunLoop::Quit),
                        base::test::RunOnceCallback<5>(result)));
    filler->FillForm(manager, kUsername, kOldPassword, kNewPassword,
                     std::move(callback));
    run_loop.Run();
  }

  password_manager::PasswordForm* existing_credential() {
    return &existing_credential_;
  }

  GURL url() const { return GURL(kUrlString); }

  password_manager::PasswordManagerClient* client() { return &client_; }

  MockStubPasswordManagerDriver& driver() { return driver_; }
  password_manager::FakeFormFetcher& form_fetcher() { return form_fetcher_; }
  const std::unique_ptr<ModelQualityLogsUploader>& logs_uploader() const {
    return logs_uploader_;
  }

  password_manager::MockPasswordStoreInterface* profile_password_store() {
    return password_store_.get();
  }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
  MockChromePasswordManagerClient client_;
  scoped_refptr<password_manager::MockPasswordStoreInterface> password_store_ =
      base::MakeRefCounted<password_manager::MockPasswordStoreInterface>();
  password_manager::FakeFormFetcher form_fetcher_;
  password_manager::MockPasswordManager mock_manager_;
  password_manager::MockPasswordFormCache mock_cache_;
  std::unique_ptr<ModelQualityLogsUploader> logs_uploader_;
  MockStubPasswordManagerDriver driver_;
  password_manager::PasswordForm existing_credential_;
  std::vector<std::unique_ptr<password_manager::PasswordFormManager>> managers_;
};

TEST_F(ChangePasswordFormFillerTest, SucceededForExistingCredential) {
  auto* form_manager =
      CreateFormManager(/*credentials_to_seed=*/{*existing_credential()});

  base::test::TestFuture<FillingResult> filling_future;
  auto filler = CreateFiller();

  password_manager::StoredCredential presaved_generated_password_form;
  EXPECT_CALL(*profile_password_store(), UpdateLogin)
      .WillOnce(MoveArg<0>(&presaved_generated_password_form));

  CompleteFormFilling(form_manager, filler.get(),
                      CreateFilledTestPasswordFormData(),
                      filling_future.GetCallback());

  EXPECT_TRUE(filling_future.Get().has_value());
  EXPECT_EQ(presaved_generated_password_form.username_value,
            existing_credential()->username_value);
  EXPECT_EQ(presaved_generated_password_form.password_value,
            existing_credential()->password_value);
  EXPECT_EQ(presaved_generated_password_form.GetPasswordBackup(), kNewPassword);

  const auto& final_log = logs_uploader()->GetFinalLog();
  EXPECT_TRUE(final_log.password_change_submission()
                  .quality()
                  .has_change_password_form_data());
  EXPECT_EQ(final_log.password_change_submission()
                .quality()
                .change_password_form_data()
                .url(),
            kUrlString);
}

TEST_F(ChangePasswordFormFillerTest, SucceededNewCredential) {
  auto* form_manager = CreateFormManager(/*credentials_to_seed=*/{});

  base::test::TestFuture<FillingResult> filling_future;
  auto filler = CreateFiller();

  password_manager::StoredCredential presaved_generated_password_form;
  EXPECT_CALL(*profile_password_store(), AddLogin)
      .WillOnce(MoveArg<0>(&presaved_generated_password_form));

  CompleteFormFilling(form_manager, filler.get(),
                      CreateFilledTestPasswordFormData(),
                      filling_future.GetCallback());

  EXPECT_TRUE(filling_future.Get().has_value());
  EXPECT_EQ(presaved_generated_password_form.username_value, kUsername);
  EXPECT_EQ(presaved_generated_password_form.password_value, kOldPassword);
  EXPECT_EQ(presaved_generated_password_form.GetPasswordBackup(), kNewPassword);
}

TEST_F(ChangePasswordFormFillerTest,
       PresaveGeneratedPasswordForDifferentInputPassword) {
  password_manager::PasswordForm* stored_form = existing_credential();
  stored_form->password_value = u"stored_password";
  auto* form_manager =
      CreateFormManager(/*credentials_to_seed=*/{*stored_form});

  base::test::TestFuture<FillingResult> filling_future;
  auto filler = CreateFiller();

  password_manager::StoredCredential presaved_generated_password_form;
  EXPECT_CALL(*profile_password_store(), UpdateLogin)
      .WillOnce(MoveArg<0>(&presaved_generated_password_form));

  CompleteFormFilling(form_manager, filler.get(),
                      CreateFilledTestPasswordFormData(),
                      filling_future.GetCallback());

  EXPECT_TRUE(filling_future.Get().has_value());
  EXPECT_EQ(presaved_generated_password_form.username_value,
            stored_form->username_value);
  EXPECT_EQ(presaved_generated_password_form.password_value,
            stored_form->password_value);
  EXPECT_EQ(presaved_generated_password_form.GetPasswordBackup(), kNewPassword);
}

TEST_F(ChangePasswordFormFillerTest, FailedFilling) {
  auto* form_manager = CreateFormManager(/*credentials_to_seed=*/{});

  base::test::TestFuture<FillingResult> filling_future;
  auto filler = CreateFiller();

  CompleteFormFilling(form_manager, filler.get(), std::nullopt,
                      filling_future.GetCallback());

  EXPECT_FALSE(filling_future.IsReady());
  EXPECT_TRUE(filler->form_waiter());

  const auto& final_log = logs_uploader()->GetFinalLog();
  EXPECT_EQ(
      final_log.password_change_submission().quality().submit_form().status(),
      ModelQualityLogsUploader::QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FORM_FILLING_FAILED);
}

TEST_F(ChangePasswordFormFillerTest, FailedFillingFormWaiterTimeout) {
  auto* form_manager = CreateFormManager(/*credentials_to_seed=*/{});

  base::test::TestFuture<FillingResult> filling_future;
  auto filler = CreateFiller();

  CompleteFormFilling(form_manager, filler.get(), std::nullopt,
                      filling_future.GetCallback());

  ASSERT_TRUE(filler->form_waiter());
  task_environment()->AdvanceClock(base::Seconds(10));

  EXPECT_EQ(filling_future.Get().error(), SubmissionError::kFailedToFillForm);

  const auto& final_log = logs_uploader()->GetFinalLog();
  EXPECT_EQ(
      final_log.password_change_submission().quality().submit_form().status(),
      ModelQualityLogsUploader::QualityStatus::
          PasswordChangeQuality_StepQuality_SubmissionStatus_FORM_FILLING_FAILED);
}

TEST_F(ChangePasswordFormFillerTest, ProvisionallySaveFailed) {
  auto* form_manager =
      CreateFormManager(/*credentials_to_seed=*/{*existing_credential()});

  base::test::TestFuture<FillingResult> filling_future;
  auto filler = CreateFiller();

  CompleteFormFilling(form_manager, filler.get(),
                      CreateEmptyTestPasswordFormData(),
                      filling_future.GetCallback());

  EXPECT_TRUE(filler->form_waiter());

  autofill::FormData new_form_data =
      CreateTestPasswordFormData("", "", 101, 102);
  new_form_data.set_renderer_id(autofill::test::MakeFormRendererId());
  auto* new_form_manager =
      CreateFormManagerFromFormData(new_form_data, /*credentials_to_seed=*/{});

  autofill::FormData filled_form = CreateFilledTestPasswordFormData();
  filled_form.set_renderer_id(new_form_data.renderer_id());

  EXPECT_CALL(driver(), FillChangePasswordForm)
      .WillOnce(base::test::RunOnceCallback<5>(filled_form));
  static_cast<password_manager::PasswordFormManagerObserver*>(
      filler->form_waiter())
      ->OnPasswordFormParsed(new_form_manager);

  EXPECT_TRUE(filling_future.Get().has_value());
}

TEST_F(ChangePasswordFormFillerTest,
       WhenFormFillingFailedHelpersLooksForNewForm) {
  auto* form_manager = CreateFormManager(/*credentials_to_seed=*/{});

  base::test::TestFuture<FillingResult> filling_future;
  auto filler = CreateFiller();

  CompleteFormFilling(form_manager, filler.get(), std::nullopt,
                      filling_future.GetCallback());

  EXPECT_TRUE(filler->form_waiter());
  autofill::FormData new_form_data =
      CreateTestPasswordFormData("", "", 101, 102);
  new_form_data.set_renderer_id(autofill::test::MakeFormRendererId());
  auto* new_form_manager =
      CreateFormManagerFromFormData(new_form_data, /*credentials_to_seed=*/{});

  autofill::FormData filled_form = CreateFilledTestPasswordFormData();
  filled_form.set_renderer_id(new_form_data.renderer_id());

  EXPECT_CALL(driver(), FillChangePasswordForm)
      .WillOnce(base::test::RunOnceCallback<5>(filled_form));
  static_cast<password_manager::PasswordFormManagerObserver*>(
      filler->form_waiter())
      ->OnPasswordFormParsed(new_form_manager);

  EXPECT_TRUE(filling_future.Get().has_value());
}

TEST_F(ChangePasswordFormFillerTest,
       WhenFormFillingFailedItIgnoresTheSameForm) {
  auto* form_manager = CreateFormManager(/*credentials_to_seed=*/{});

  base::test::TestFuture<FillingResult> filling_future;
  auto filler = CreateFiller();

  CompleteFormFilling(form_manager, filler.get(), std::nullopt,
                      filling_future.GetCallback());

  ASSERT_TRUE(filler->form_waiter());

  EXPECT_CALL(driver(), FillChangePasswordForm).Times(0);
  static_cast<password_manager::PasswordFormManagerObserver*>(
      filler->form_waiter())
      ->OnPasswordFormParsed(form_manager);
  testing::Mock::VerifyAndClearExpectations(&driver());
}
