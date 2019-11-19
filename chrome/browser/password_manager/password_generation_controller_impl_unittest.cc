// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/password_manager/password_generation_controller_impl.h"

#include <map>
#include <utility>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/autofill/mock_manual_filling_controller.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_generation_dialog_view_interface.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_generation_frame_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"

using autofill::password_generation::PasswordGenerationType;
using password_manager::metrics_util::GenerationDialogChoice;

namespace {
using autofill::FooterCommand;
using autofill::PasswordForm;
using autofill::mojom::FocusedFieldType;
using autofill::password_generation::PasswordGenerationUIData;
using base::ASCIIToUTF16;
using password_manager::MockPasswordStore;
using testing::_;
using testing::ByMove;
using testing::Eq;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

class TestPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  TestPasswordManagerClient();
  ~TestPasswordManagerClient() override;

  password_manager::PasswordStore* GetProfilePasswordStore() const override;

 private:
  scoped_refptr<MockPasswordStore> mock_password_store_;
};

TestPasswordManagerClient::TestPasswordManagerClient() {
  mock_password_store_ = new MockPasswordStore();
}

TestPasswordManagerClient::~TestPasswordManagerClient() {
  mock_password_store_->ShutdownOnUIThread();
}

password_manager::PasswordStore*
TestPasswordManagerClient::GetProfilePasswordStore() const {
  return mock_password_store_.get();
}

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MockPasswordManagerDriver() = default;

  MOCK_METHOD1(GeneratedPasswordAccepted, void(const base::string16&));
  MOCK_METHOD0(GetPasswordGenerationHelper,
               password_manager::PasswordGenerationFrameHelper*());
  MOCK_METHOD0(GetPasswordManager, password_manager::PasswordManager*());
  MOCK_METHOD0(GetPasswordAutofillManager,
               password_manager::PasswordAutofillManager*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordManagerDriver);
};

class MockPasswordGenerationHelper
    : public password_manager::PasswordGenerationFrameHelper {
 public:
  MockPasswordGenerationHelper(password_manager::PasswordManagerClient* client,
                               password_manager::PasswordManagerDriver* driver)
      : password_manager::PasswordGenerationFrameHelper(client, driver) {}

  MOCK_METHOD5(GeneratePassword,
               base::string16(const GURL&,
                              autofill::FormSignature,
                              autofill::FieldSignature,
                              uint32_t,
                              uint32_t*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordGenerationHelper);
};

// Mock modal dialog view used to bypass the need of a valid top level window.
class MockPasswordGenerationDialogView
    : public PasswordGenerationDialogViewInterface {
 public:
  MockPasswordGenerationDialogView() = default;

  MOCK_METHOD3(Show,
               void(base::string16&,
                    base::WeakPtr<password_manager::PasswordManagerDriver>,
                    PasswordGenerationType));
  MOCK_METHOD0(Destroy, void());

  virtual ~MockPasswordGenerationDialogView() { Destroy(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordGenerationDialogView);
};

PasswordGenerationUIData GetTestGenerationUIData1() {
  PasswordGenerationUIData data;

  PasswordForm& form = data.password_form;
  form.form_data = autofill::FormData();
  form.form_data.action = GURL("http://www.example1.com/accounts/Login");
  form.form_data.url = GURL("http://www.example1.com/accounts/LoginAuth");

  data.generation_element = ASCIIToUTF16("testelement1");
  data.max_length = 10;

  return data;
}

PasswordGenerationUIData GetTestGenerationUIData2() {
  PasswordGenerationUIData data;

  PasswordForm& form = data.password_form;
  form.form_data = autofill::FormData();
  form.form_data.action = GURL("http://www.example2.com/accounts/Login");
  form.form_data.url = GURL("http://www.example2.com/accounts/LoginAuth");

  data.generation_element = ASCIIToUTF16("testelement2");
  data.max_length = 10;

  return data;
}

MATCHER_P(PointsToSameAddress, expected, "") {
  return arg.get() == expected;
}

}  // namespace

class PasswordGenerationControllerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    test_pwd_manager_client_ = std::make_unique<TestPasswordManagerClient>();
    PasswordGenerationControllerImpl::CreateForWebContentsForTesting(
        web_contents(), test_pwd_manager_client_.get(),
        mock_manual_filling_controller_.AsWeakPtr(),
        mock_dialog_factory_.Get());

    password_manager_ = std::make_unique<password_manager::PasswordManager>(
        test_pwd_manager_client_.get());
    mock_password_manager_driver_ =
        std::make_unique<NiceMock<MockPasswordManagerDriver>>();

    // TODO(crbug.com/969051): Remove once kAutofillKeyboardAccessory is
    // enabled.
    password_autofill_manager_ =
        std::make_unique<password_manager::PasswordAutofillManager>(
            mock_password_manager_driver_.get(), &test_autofill_client_,
            test_pwd_manager_client_.get());

    ON_CALL(*mock_password_manager_driver_, GetPasswordManager())
        .WillByDefault(Return(password_manager_.get()));
    ON_CALL(*mock_password_manager_driver_, GetPasswordAutofillManager())
        .WillByDefault(Return(password_autofill_manager_.get()));

    mock_generation_helper_ =
        std::make_unique<NiceMock<MockPasswordGenerationHelper>>(
            nullptr, mock_password_manager_driver_.get());
    mock_dialog_ =
        std::make_unique<NiceMock<MockPasswordGenerationDialogView>>();

    EXPECT_CALL(mock_manual_filling_controller_,
                OnAutomaticGenerationStatusChanged(false));
    controller()->FocusedInputChanged(
        FocusedFieldType::kFillablePasswordField,
        base::AsWeakPtr(mock_password_manager_driver_.get()));
  }

  PasswordGenerationController* controller() {
    return PasswordGenerationControllerImpl::FromWebContents(web_contents());
  }

  const base::MockCallback<
      PasswordGenerationControllerImpl::CreateDialogFactory>&
  mock_dialog_factory() {
    return mock_dialog_factory_;
  }

 protected:
  // Sets up mocks needed by the generation flow and signals the
  // |PasswordGenerationController| that generation is available.
  void InitializeAutomaticGeneration(const base::string16& password);

  // Sets up mocks needed by the generation flow.
  void InitializeManualGeneration(const base::string16& password);

  StrictMock<MockManualFillingController> mock_manual_filling_controller_;

  std::unique_ptr<NiceMock<MockPasswordManagerDriver>>
      mock_password_manager_driver_;
  std::unique_ptr<NiceMock<MockPasswordGenerationHelper>>
      mock_generation_helper_;
  std::unique_ptr<NiceMock<MockPasswordGenerationDialogView>> mock_dialog_;

 private:
  NiceMock<
      base::MockCallback<PasswordGenerationControllerImpl::CreateDialogFactory>>
      mock_dialog_factory_;
  std::unique_ptr<password_manager::PasswordManager> password_manager_;
  std::unique_ptr<password_manager::PasswordAutofillManager>
      password_autofill_manager_;
  std::unique_ptr<TestPasswordManagerClient> test_pwd_manager_client_;
  autofill::TestAutofillClient test_autofill_client_;
};

void PasswordGenerationControllerTest::InitializeAutomaticGeneration(
    const base::string16& password) {
  ON_CALL(*mock_password_manager_driver_, GetPasswordGenerationHelper())
      .WillByDefault(Return(mock_generation_helper_.get()));

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(true));

  controller()->OnAutomaticGenerationAvailable(
      mock_password_manager_driver_.get(), GetTestGenerationUIData1(),
      gfx::RectF(100, 20));

  ON_CALL(*mock_generation_helper_, GeneratePassword(_, _, _, _, _))
      .WillByDefault(Return(password));
}

void PasswordGenerationControllerTest::InitializeManualGeneration(
    const base::string16& password) {
  ON_CALL(*mock_password_manager_driver_, GetPasswordGenerationHelper())
      .WillByDefault(Return(mock_generation_helper_.get()));

  ON_CALL(*mock_generation_helper_, GeneratePassword(_, _, _, _, _))
      .WillByDefault(Return(password));
}

TEST_F(PasswordGenerationControllerTest, IsNotRecreatedForSameWebContents) {
  PasswordGenerationController* initial_controller =
      PasswordGenerationControllerImpl::FromWebContents(web_contents());
  EXPECT_NE(nullptr, initial_controller);
  PasswordGenerationControllerImpl::CreateForWebContents(web_contents());
  EXPECT_EQ(PasswordGenerationControllerImpl::FromWebContents(web_contents()),
            initial_controller);
}

TEST_F(PasswordGenerationControllerTest, RelaysAutomaticGenerationAvailable) {
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(true));
  controller()->OnAutomaticGenerationAvailable(
      mock_password_manager_driver_.get(), GetTestGenerationUIData1(),
      gfx::RectF(100, 20));
}

// Tests that if AutomaticGenerationAvailable is called for different
// password forms, the form and field signatures used for password generation
// are updated.
TEST_F(PasswordGenerationControllerTest,
       UpdatesSignaturesForDifferentGenerationForms) {
  // Called twice for different forms.
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(true))
      .Times(2);
  controller()->OnAutomaticGenerationAvailable(
      mock_password_manager_driver_.get(), GetTestGenerationUIData1(),
      gfx::RectF(100, 20));
  PasswordGenerationUIData new_ui_data = GetTestGenerationUIData2();
  controller()->OnAutomaticGenerationAvailable(
      mock_password_manager_driver_.get(), new_ui_data, gfx::RectF(100, 20));

  autofill::FormSignature form_signature =
      autofill::CalculateFormSignature(new_ui_data.password_form.form_data);
  autofill::FieldSignature field_signature =
      autofill::CalculateFieldSignatureByNameAndType(
          new_ui_data.generation_element, "password");

  base::string16 generated_password = ASCIIToUTF16("t3stp@ssw0rd");
  NiceMock<MockPasswordGenerationDialogView>* raw_dialog_view =
      mock_dialog_.get();
  EXPECT_CALL(mock_dialog_factory(), Run)
      .WillOnce(Return(ByMove(std::move(mock_dialog_))));
  EXPECT_CALL(*mock_password_manager_driver_, GetPasswordGenerationHelper())
      .WillOnce(Return(mock_generation_helper_.get()));
  EXPECT_CALL(*mock_generation_helper_,
              GeneratePassword(_, form_signature, field_signature,
                               uint32_t(new_ui_data.max_length), _))
      .WillOnce(Return(generated_password));
  EXPECT_CALL(*raw_dialog_view,
              Show(generated_password,
                   PointsToSameAddress(mock_password_manager_driver_.get()),
                   PasswordGenerationType::kAutomatic));
  controller()->OnGenerationRequested(PasswordGenerationType::kAutomatic);
}

TEST_F(PasswordGenerationControllerTest,
       RecordsGeneratedPasswordAcceptedAutomatic) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(true));
  controller()->OnAutomaticGenerationAvailable(
      mock_password_manager_driver_.get(), GetTestGenerationUIData1(),
      gfx::RectF(100, 20));

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(false));
  controller()->GeneratedPasswordAccepted(
      ASCIIToUTF16("t3stp@ssw0rd"), mock_password_manager_driver_->AsWeakPtr(),
      PasswordGenerationType::kAutomatic);

  histogram_tester.ExpectUniqueSample(
      "KeyboardAccessory.GenerationDialogChoice.Automatic",
      GenerationDialogChoice::kAccepted, 1);
}

TEST_F(PasswordGenerationControllerTest,
       RecordsGeneratedPasswordRejectedAutomatic) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(false));
  controller()->GeneratedPasswordRejected(PasswordGenerationType::kAutomatic);

  histogram_tester.ExpectUniqueSample(
      "KeyboardAccessory.GenerationDialogChoice.Automatic",
      GenerationDialogChoice::kRejected, 1);
}

TEST_F(PasswordGenerationControllerTest,
       RecordsGeneratedPasswordAcceptedManual) {
  base::HistogramTester histogram_tester;

  InitializeManualGeneration(ASCIIToUTF16("t3stp@ssw0rd"));
  controller()->OnGenerationRequested(PasswordGenerationType::kManual);

  EXPECT_CALL(mock_dialog_factory(), Run)
      .WillOnce(Return(ByMove(std::move(mock_dialog_))));
  controller()->ShowManualGenerationDialog(mock_password_manager_driver_.get(),
                                           GetTestGenerationUIData1());

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(false));
  controller()->GeneratedPasswordAccepted(
      ASCIIToUTF16("t3stp@ssw0rd"), mock_password_manager_driver_->AsWeakPtr(),
      PasswordGenerationType::kManual);

  histogram_tester.ExpectUniqueSample(
      "KeyboardAccessory.GenerationDialogChoice.Manual",
      GenerationDialogChoice::kAccepted, 1);
}

TEST_F(PasswordGenerationControllerTest,
       RecordsGeneratedPasswordRejectedManual) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(false));
  controller()->GeneratedPasswordRejected(PasswordGenerationType::kManual);

  histogram_tester.ExpectUniqueSample(
      "KeyboardAccessory.GenerationDialogChoice.Manual",
      GenerationDialogChoice::kRejected, 1);
}

TEST_F(PasswordGenerationControllerTest,
       RejectAutomaticAvailableForNonActiveFrame) {
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(_))
      .Times(0);
  MockPasswordManagerDriver wrong_driver;
  controller()->OnAutomaticGenerationAvailable(
      &wrong_driver, GetTestGenerationUIData2(), gfx::RectF(100, 20));
}

TEST_F(PasswordGenerationControllerTest,
       ResetStateWhenFocusChangesToNonPassword) {
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(false));

  MockPasswordManagerDriver new_driver;
  controller()->FocusedInputChanged(FocusedFieldType::kFillableUsernameField,
                                    base::AsWeakPtr(&new_driver));
  EXPECT_FALSE(controller()->GetActiveFrameDriver());
}

TEST_F(PasswordGenerationControllerTest,
       ResetStateWhenFocusChangesToOtherFramePassword) {
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(false));

  MockPasswordManagerDriver new_driver;
  controller()->FocusedInputChanged(FocusedFieldType::kFillablePasswordField,
                                    base::AsWeakPtr(&new_driver));
  EXPECT_EQ(&new_driver, controller()->GetActiveFrameDriver().get());
}

TEST_F(PasswordGenerationControllerTest, HidesDialogWhenFocusChanges) {
  base::string16 test_password = ASCIIToUTF16("t3stp@ssw0rd");
  InitializeManualGeneration(test_password);
  controller()->OnGenerationRequested(PasswordGenerationType::kManual);

  NiceMock<MockPasswordGenerationDialogView>* raw_dialog_view =
      mock_dialog_.get();
  EXPECT_CALL(mock_dialog_factory(), Run)
      .WillOnce(Return(ByMove(std::move(mock_dialog_))));
  EXPECT_CALL(*raw_dialog_view,
              Show(test_password,
                   PointsToSameAddress(mock_password_manager_driver_.get()),
                   PasswordGenerationType::kManual));
  controller()->ShowManualGenerationDialog(mock_password_manager_driver_.get(),
                                           GetTestGenerationUIData1());
  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(false));
  EXPECT_CALL(*raw_dialog_view, Destroy());
  controller()->FocusedInputChanged(
      FocusedFieldType::kFillableUsernameField,
      base::AsWeakPtr(mock_password_manager_driver_.get()));
  Mock::VerifyAndClearExpectations(raw_dialog_view);
}

TEST_F(PasswordGenerationControllerTest, ShowManualDialogForActiveFrame) {
  base::string16 test_password = ASCIIToUTF16("t3stp@ssw0rd");
  InitializeManualGeneration(test_password);
  controller()->OnGenerationRequested(PasswordGenerationType::kManual);

  NiceMock<MockPasswordGenerationDialogView>* raw_dialog_view =
      mock_dialog_.get();
  EXPECT_CALL(mock_dialog_factory(), Run)
      .WillOnce(Return(ByMove(std::move(mock_dialog_))));
  EXPECT_CALL(*raw_dialog_view,
              Show(test_password,
                   PointsToSameAddress(mock_password_manager_driver_.get()),
                   PasswordGenerationType::kManual));
  controller()->ShowManualGenerationDialog(mock_password_manager_driver_.get(),
                                           GetTestGenerationUIData1());
}

TEST_F(PasswordGenerationControllerTest,
       RejectShowManualDialogForNonActiveFrame) {
  MockPasswordManagerDriver wrong_driver;
  EXPECT_CALL(mock_dialog_factory(), Run).Times(0);
  controller()->ShowManualGenerationDialog(&wrong_driver,
                                           GetTestGenerationUIData1());
}

TEST_F(PasswordGenerationControllerTest, DontShowDialogIfAlreadyShown) {
  base::string16 test_password = ASCIIToUTF16("t3stp@ssw0rd");
  InitializeManualGeneration(test_password);
  controller()->OnGenerationRequested(PasswordGenerationType::kManual);

  NiceMock<MockPasswordGenerationDialogView>* raw_dialog_view =
      mock_dialog_.get();
  EXPECT_CALL(mock_dialog_factory(), Run)
      .WillOnce(Return(ByMove(std::move(mock_dialog_))));

  EXPECT_CALL(*raw_dialog_view,
              Show(test_password,
                   PointsToSameAddress(mock_password_manager_driver_.get()),
                   PasswordGenerationType::kManual));
  controller()->ShowManualGenerationDialog(mock_password_manager_driver_.get(),
                                           GetTestGenerationUIData1());

  EXPECT_CALL(mock_dialog_factory(), Run).Times(0);
  controller()->ShowManualGenerationDialog(mock_password_manager_driver_.get(),
                                           GetTestGenerationUIData1());
}

TEST_F(PasswordGenerationControllerTest, DontShowManualDialogIfFocusChanged) {
  InitializeManualGeneration(ASCIIToUTF16("t3stp@ssw0rd"));
  controller()->OnGenerationRequested(PasswordGenerationType::kManual);

  EXPECT_CALL(mock_manual_filling_controller_,
              OnAutomaticGenerationStatusChanged(false));
  controller()->FocusedInputChanged(
      FocusedFieldType::kFillablePasswordField,
      base::AsWeakPtr(mock_password_manager_driver_.get()));
  EXPECT_CALL(mock_dialog_factory(), Run).Times(0);
  controller()->ShowManualGenerationDialog(mock_password_manager_driver_.get(),
                                           GetTestGenerationUIData1());
}
