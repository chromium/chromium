// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/otp_detection_helper.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/content_autofill_driver_factory_test_api.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/one_time_passwords/otp_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

// Mock for PasswordManagerClient to control its GetOtpManager method.
class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(password_manager::OtpManager*, GetOtpManager, (), (override));
};

class TestAutofillManager : public autofill::BrowserAutofillManager {
 public:
  explicit TestAutofillManager(autofill::ContentAutofillDriver* driver)
      : BrowserAutofillManager(driver) {}

  testing::AssertionResult WaitForFormsSeen(int min_num_awaited_calls) {
    return forms_seen_waiter_.Wait(min_num_awaited_calls);
  }

 private:
  autofill::TestAutofillManagerWaiter forms_seen_waiter_{
      *this,
      {autofill::AutofillManagerEvent::kFormsSeen}};
};

}  // namespace

class OtpDetectionHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  OtpDetectionHelperTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~OtpDetectionHelperTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    content::NavigationSimulator::CreateBrowserInitiated(GURL("https://a.com/"),
                                                         web_contents())
        ->Commit();

    ON_CALL(client_, GetOtpManager()).WillByDefault(Return(&otp_manager_));
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  autofill::FormData CreateSimpleOtp() {
    content::RenderFrameHost* rfh = web_contents()->GetPrimaryMainFrame();
    autofill::LocalFrameToken frame_token(rfh->GetFrameToken().value());
    autofill::FormData form;
    form.set_url(GURL("https://www.foo.com"));
    form.set_renderer_id(autofill::test::MakeFormRendererId());
    autofill::FormFieldData field = {autofill::test::CreateTestFormField(
        "some_label", "some_name", "some_value",
        autofill::FormControlType::kInputText)};

    form.set_fields({field});
    return autofill::test::CreateFormDataForFrame(form, frame_token);
  }

  void AddOtpToThePage() {
    auto form = CreateSimpleOtp();

    GetAutofillManager()->OnFormsSeen(
        /*updated_forms=*/{form},
        /*removed_forms=*/{});
    ASSERT_TRUE(GetAutofillManager()->WaitForFormsSeen(1));
    ASSERT_TRUE(
        GetAutofillManager()->FindCachedFormById(form.fields()[0].global_id()));

    otp_manager()->ProcessClassificationModelPredictions(
        form, {{form.fields()[0].global_id(), autofill::ONE_TIME_CODE}});
    visible_forms_.push_back(form.global_id());
  }

  void RemoveOtpFromThePage() {
    GetAutofillManager()->OnFormsSeen(
        /*updated_forms=*/{},
        /*removed_forms=*/{visible_forms_});
    visible_forms_.clear();
  }

  MockPasswordManagerClient* client() { return &client_; }
  password_manager::OtpManager* otp_manager() { return &otp_manager_; }

  TestAutofillManager* GetAutofillManager() {
    return autofill_manager_injector_[web_contents()->GetPrimaryMainFrame()];
  }

  autofill::TestContentAutofillClient* autofill_client() {
    return autofill_client_injector_[web_contents()];
  }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      autofill_client_injector_;
  autofill::TestAutofillManagerInjector<TestAutofillManager>
      autofill_manager_injector_;
  NiceMock<MockPasswordManagerClient> client_;
  password_manager::OtpManager otp_manager_{&client_};

  std::vector<autofill::FormGlobalId> visible_forms_;
};

TEST_F(OtpDetectionHelperTest, CheckOTPPresence) {
  EXPECT_FALSE(OtpDetectionHelper::IsOtpPresent(web_contents(), client()));

  AddOtpToThePage();

  EXPECT_TRUE(OtpDetectionHelper::IsOtpPresent(web_contents(), client()));

  RemoveOtpFromThePage();

  EXPECT_FALSE(OtpDetectionHelper::IsOtpPresent(web_contents(), client()));
}

TEST_F(OtpDetectionHelperTest, OtpDetectedInitiallyAndStillPresent) {
  AddOtpToThePage();

  base::MockOnceCallback<void()> completion_callback_;
  EXPECT_CALL(completion_callback_, Run()).Times(0);
  OtpDetectionHelper helper(web_contents(), client(),
                            completion_callback_.Get());
}

TEST_F(OtpDetectionHelperTest, CallbackInvokedAfterNavigationClearsOtps) {
  AddOtpToThePage();

  base::MockOnceCallback<void()> completion_callback_;
  EXPECT_CALL(completion_callback_, Run()).Times(0);

  OtpDetectionHelper helper(web_contents(), client(),
                            completion_callback_.Get());

  // Simulate a navigation. The field is still present.
  helper.DidFinishNavigation(nullptr);
  EXPECT_CALL(completion_callback_, Run()).Times(0);

  // Now, make the field disappear and simulate another navigation.
  RemoveOtpFromThePage();
  EXPECT_CALL(completion_callback_, Run()).Times(1);
  helper.DidFinishNavigation(nullptr);
}

TEST_F(OtpDetectionHelperTest, OtpFieldDetectedAndStillPresentAfterNavigation) {
  base::MockOnceCallback<void()> completion_callback_;
  EXPECT_CALL(completion_callback_, Run()).Times(0);
  AddOtpToThePage();
  OtpDetectionHelper helper(web_contents(), client(),
                            completion_callback_.Get());

  // Simulate navigation. Make the field disappear before navigation is
  // processed.
  EXPECT_CALL(completion_callback_, Run()).Times(0);
  helper.DidFinishNavigation(nullptr);
}

TEST_F(OtpDetectionHelperTest, MultipleOtpFieldsDetectedAndSomeDisappear) {
  base::MockOnceCallback<void()> completion_callback_;
  EXPECT_CALL(completion_callback_, Run()).Times(0);
  AddOtpToThePage();

  OtpDetectionHelper helper(web_contents(), client(),
                            completion_callback_.Get());

  // Remove OTP, then add a new one. This simulates a case when user entered a
  // wrong OTP and a the form reappears.
  RemoveOtpFromThePage();
  AddOtpToThePage();

  // Simulate a navigation. The field is still present.
  helper.DidFinishNavigation(nullptr);
}
