// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"

#include "base/test/gmock_move_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/fast_checkout/fast_checkout_external_action_delegate.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/public/mock_headless_script_controller.h"
#include "ui/gfx/native_widget_types.h"

namespace {
constexpr char kUrl[] = "https://www.example.com";
}

class MockFastCheckoutController : public FastCheckoutController {
 public:
  MockFastCheckoutController() : FastCheckoutController() {}
  ~MockFastCheckoutController() override = default;

  MOCK_METHOD(void, Show, (), (override));
  MOCK_METHOD(void,
              OnOptionsSelected,
              (std::unique_ptr<autofill::AutofillProfile> profile,
               std::unique_ptr<autofill::CreditCard> credit_card),
              (override));
  MOCK_METHOD(void, OnDismiss, (), (override));
  MOCK_METHOD(gfx::NativeView, GetNativeView, (), (override));
};

class MockFastCheckoutExternalActionDelegate
    : public FastCheckoutExternalActionDelegate {
 public:
  MockFastCheckoutExternalActionDelegate() = default;
  ~MockFastCheckoutExternalActionDelegate() override = default;

  MOCK_METHOD(void,
              SetOptionsSelected,
              (const autofill::AutofillProfile& selected_profile,
               const autofill::CreditCard& selected_credit_card),
              (override));
};

class TestFastCheckoutClientImpl : public FastCheckoutClientImpl {
 public:
  static TestFastCheckoutClientImpl* CreateForWebContents(
      content::WebContents* web_contents);

  explicit TestFastCheckoutClientImpl(content::WebContents* web_contents)
      : FastCheckoutClientImpl(web_contents) {}

  std::unique_ptr<autofill_assistant::HeadlessScriptController>
  CreateHeadlessScriptController() override {
    return std::move(external_script_controller_);
  }

  void InjectHeadlessScriptControllerForTesting(
      std::unique_ptr<autofill_assistant::HeadlessScriptController>
          external_script_controller) {
    external_script_controller_ = std::move(external_script_controller);
  }

  std::unique_ptr<FastCheckoutController> CreateFastCheckoutController()
      override {
    return std::move(fast_checkout_controller_);
  }

  void InjectFastCheckoutController(
      std::unique_ptr<FastCheckoutController> fast_checkout_controller) {
    fast_checkout_controller_ = std::move(fast_checkout_controller);
  }

  std::unique_ptr<FastCheckoutExternalActionDelegate>
  CreateFastCheckoutExternalActionDelegate() override {
    return std::move(external_action_delegate_);
  }

  void InjectFastCheckoutExternalActionDelegate(
      std::unique_ptr<FastCheckoutExternalActionDelegate>
          external_action_delegate) {
    external_action_delegate_ = std::move(external_action_delegate);
  }

 private:
  std::unique_ptr<autofill_assistant::HeadlessScriptController>
      external_script_controller_;
  std::unique_ptr<FastCheckoutController> fast_checkout_controller_;
  std::unique_ptr<FastCheckoutExternalActionDelegate> external_action_delegate_;
};

// static
TestFastCheckoutClientImpl* TestFastCheckoutClientImpl::CreateForWebContents(
    content::WebContents* web_contents) {
  const void* key = WebContentsUserData<FastCheckoutClientImpl>::UserDataKey();
  web_contents->SetUserData(
      key, std::make_unique<TestFastCheckoutClientImpl>(web_contents));
  return static_cast<TestFastCheckoutClientImpl*>(
      web_contents->GetUserData(key));
}

class FastCheckoutClientImplTest : public ChromeRenderViewHostTestHarness {
 public:
  FastCheckoutClientImplTest() {
    feature_list_.InitWithFeatures({features::kFastCheckout}, {});
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    test_client_ =
        TestFastCheckoutClientImpl::CreateForWebContents(web_contents());

    // Prepare the HeadlessScriptController.
    auto external_script_controller =
        std::make_unique<autofill_assistant::MockHeadlessScriptController>();
    external_script_controller_ = external_script_controller.get();
    test_client_->InjectHeadlessScriptControllerForTesting(
        std::move(external_script_controller));

    // Prepare the FastCheckoutController.
    auto fast_checkout_controller =
        std::make_unique<MockFastCheckoutController>();
    fast_checkout_controller_ = fast_checkout_controller.get();
    test_client_->InjectFastCheckoutController(
        std::move(fast_checkout_controller));

    // Prepare the FastCheckoutExternalActionDelegate.
    auto external_action_delegate =
        std::make_unique<MockFastCheckoutExternalActionDelegate>();
    external_action_delegate_ = external_action_delegate.get();
    test_client_->InjectFastCheckoutExternalActionDelegate(
        std::move(external_action_delegate));
  }

  TestFastCheckoutClientImpl* fast_checkout_client() { return test_client_; }

  autofill_assistant::MockHeadlessScriptController*
  external_script_controller() {
    return external_script_controller_;
  }

  MockFastCheckoutController* fast_checkout_controller() {
    return fast_checkout_controller_;
  }

  MockFastCheckoutExternalActionDelegate* delegate() {
    return external_action_delegate_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  raw_ptr<TestFastCheckoutClientImpl> test_client_;
  raw_ptr<autofill_assistant::MockHeadlessScriptController>
      external_script_controller_;
  raw_ptr<MockFastCheckoutController> fast_checkout_controller_;
  raw_ptr<MockFastCheckoutExternalActionDelegate> external_action_delegate_;
};

TEST_F(
    FastCheckoutClientImplTest,
    GetOrCreateForWebContents_ClientWasAlreadyCreated_ReturnsExistingInstance) {
  raw_ptr<FastCheckoutClient> client =
      FastCheckoutClient::GetOrCreateForWebContents(web_contents());

  // There is only one client per `WebContents`.
  EXPECT_EQ(client, fast_checkout_client());
}

TEST_F(FastCheckoutClientImplTest, Start_FeatureDisabled_NoRuns) {
  // Disable Fast Checkout feature
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kFastCheckout});

  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Do not expect bottomsheet to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show).Times(0);

  // Starting is not successful which is also represented by the internal state.
  EXPECT_FALSE(fast_checkout_client()->Start(GURL(kUrl)));
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest, Start_FeatureEnabled_RunsSuccessfully) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Prepare to extract the callback to the external script controller.
  base::OnceCallback<void(
      autofill_assistant::HeadlessScriptController::ScriptResult)>
      external_script_controller_callback;
  EXPECT_CALL(*external_script_controller(), StartScript)
      .Times(1)
      .WillOnce(MoveArg<1>(&external_script_controller_callback));

  // Expect bottomsheet to show up.
  EXPECT_CALL(*fast_checkout_controller(), Show);

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(GURL(kUrl)));

  // `FastCheckoutClient` is running.
  EXPECT_TRUE(fast_checkout_client()->IsRunning());

  // Cannot start another run.
  EXPECT_FALSE(fast_checkout_client()->Start(GURL(kUrl)));

  // Successful run.
  autofill_assistant::HeadlessScriptController::ScriptResult script_result = {
      /* success= */ true};
  std::move(external_script_controller_callback).Run(script_result);

  // `FastCheckoutClient` state was reset after run finished.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest, Stop_WhenIsRunning_CancelsTheRun) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(GURL(kUrl)));

  fast_checkout_client()->Stop();

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest, OnDismiss_WhenIsRunning_CancelsTheRun) {
  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(GURL(kUrl)));

  fast_checkout_client()->OnDismiss();

  // `FastCheckoutClient` is not running anymore.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}

TEST_F(FastCheckoutClientImplTest,
       OnOptionsSelected_MovesSelectionsToExternalActionDelegate) {
  EXPECT_CALL(*delegate(), SetOptionsSelected);

  // Starting the run successfully.
  EXPECT_TRUE(fast_checkout_client()->Start(GURL(kUrl)));

  // User selected profile and card in bottomsheet.
  fast_checkout_client()->OnOptionsSelected(
      std::make_unique<autofill::AutofillProfile>(),
      std::make_unique<autofill::CreditCard>());
}
