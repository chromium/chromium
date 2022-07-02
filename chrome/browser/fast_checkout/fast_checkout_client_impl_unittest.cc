// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"

#include "base/test/gmock_move_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill_assistant/browser/public/mock_headless_script_controller.h"

namespace {
constexpr char kUrl[] = "https://www.example.com";
}

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

 private:
  std::unique_ptr<autofill_assistant::HeadlessScriptController>
      external_script_controller_;
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
  }

  TestFastCheckoutClientImpl* fast_checkout_client() { return test_client_; }

  autofill_assistant::MockHeadlessScriptController*
  external_script_controller() {
    return external_script_controller_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  raw_ptr<TestFastCheckoutClientImpl> test_client_;
  raw_ptr<autofill_assistant::MockHeadlessScriptController>
      external_script_controller_;
};

TEST_F(
    FastCheckoutClientImplTest,
    GetOrCreateForWebContents_ClientWasAlreadyCreated_ReturnsExistingInstance) {
  raw_ptr<FastCheckoutClient> client =
      FastCheckoutClient::GetOrCreateForWebContents(web_contents());

  // There is only one client per `WebContents`.
  EXPECT_EQ(client, fast_checkout_client());
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

TEST_F(FastCheckoutClientImplTest, Start_FeatureDisabled_NoRuns) {
  // Disable Fast Checkout feature
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kFastCheckout});

  // `FastCheckoutClient` is not running initially.
  EXPECT_FALSE(fast_checkout_client()->IsRunning());

  // Starting is not successful which is also represented by the internal state.
  EXPECT_FALSE(fast_checkout_client()->Start(GURL(kUrl)));
  EXPECT_FALSE(fast_checkout_client()->IsRunning());
}
