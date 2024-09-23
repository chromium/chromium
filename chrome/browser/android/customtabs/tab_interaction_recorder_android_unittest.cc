// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/customtabs/tab_interaction_recorder_android.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace customtabs {

namespace {

using ::autofill::AutofillManager;
using ::autofill::FormData;
using ::autofill::TestAutofillClient;
using ::autofill::TestAutofillDriver;
using ::autofill::TestBrowserAutofillManager;
using ::autofill::test::CreateTestAddressFormData;
using ::testing::_;
using ::testing::NiceMock;

void OnTextFieldDidChangeForAutofillManager(AutofillManager& autofill_manager) {
  FormData form = CreateTestAddressFormData();
  autofill_manager.OnTextFieldDidChange(form, form.fields().front().global_id(),
                                        base::TimeTicks::Now());
}

void OnFormsSeenForAutofillManager(AutofillManager& autofill_manager,
                                   content::RenderFrameHost* rfh) {
  FormData form = CreateTestAddressFormData();
  if (rfh) {
    form.set_host_frame(
        autofill::LocalFrameToken(rfh->GetFrameToken().value()));
  }
  autofill_manager.OnFormsSeen({form}, {});
}
}  // namespace

class AutofillObserverImplTest : public testing::Test {
 public:
  AutofillObserverImplTest() = default;

  void SetUp() override {
    client_.SetPrefs(autofill::test::PrefServiceForTesting());
    driver_ = std::make_unique<TestAutofillDriver>(&client_);
    driver_->set_autofill_manager(
        std::make_unique<TestBrowserAutofillManager>(driver_.get()));
  }

 protected:
  void DestroyDriver() { driver_.reset(); }

  TestBrowserAutofillManager& autofill_manager() {
    return static_cast<TestBrowserAutofillManager&>(
        driver_->GetAutofillManager());
  }

 private:
  base::test::TaskEnvironment task_environment_;
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient client_;
  std::unique_ptr<TestAutofillDriver> driver_;
};

TEST_F(AutofillObserverImplTest, TestFormInteraction) {
  base::MockOnceCallback<void(content::GlobalRenderFrameHostId)> callback;
  content::GlobalRenderFrameHostId id = content::GlobalRenderFrameHostId();
  AutofillObserverImpl observer(id, &autofill_manager(), callback.Get());

  EXPECT_CALL(callback, Run(id));
  OnTextFieldDidChangeForAutofillManager(autofill_manager());

  // Observer should no longer get notified after the first interaction.
  EXPECT_CALL(callback, Run(id)).Times(0);
  OnTextFieldDidChangeForAutofillManager(autofill_manager());
}

TEST_F(AutofillObserverImplTest, TestNoFormInteraction) {
  content::GlobalRenderFrameHostId id = content::GlobalRenderFrameHostId();
  base::MockOnceCallback<void(content::GlobalRenderFrameHostId)> callback;
  auto observer = std::make_unique<AutofillObserverImpl>(
      id, &autofill_manager(), callback.Get());

  EXPECT_CALL(callback, Run(id)).Times(0);
  observer.reset();
}

TEST_F(AutofillObserverImplTest, TestAutofillManagerDestroy) {
  content::GlobalRenderFrameHostId id = content::GlobalRenderFrameHostId();
  base::MockOnceCallback<void(content::GlobalRenderFrameHostId)> callback;
  auto observer = std::make_unique<AutofillObserverImpl>(
      id, &autofill_manager(), callback.Get());

  DestroyDriver();

  EXPECT_CALL(callback, Run(id)).Times(0);
  observer.reset();
}

// === TabInteractionRecorderAndroidTest ===

class TabInteractionRecorderAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  TabInteractionRecorderAndroidTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    client_.SetPrefs(autofill::test::PrefServiceForTesting());
    driver_ = std::make_unique<TestAutofillDriver>(&client_);
    driver_->set_autofill_manager(
        std::make_unique<TestBrowserAutofillManager>(driver_.get()));
  }

  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    std::unique_ptr<content::WebContents> contents =
        ChromeRenderViewHostTestHarness::CreateTestWebContents();
    TabInteractionRecorderAndroid::CreateForWebContents(contents.get());
    auto* helper =
        TabInteractionRecorderAndroid::FromWebContents(contents.get());
    helper->SetAutofillManagerForTest(&autofill_manager());

    // Simulate a navigation event to force the initialization of the main
    // frame.
    content::WebContentsTester::For(contents.get())
        ->NavigateAndCommit(GURL("https://foo.com"));
    task_environment()->RunUntilIdle();
    return contents;
  }

 protected:
  TestBrowserAutofillManager& autofill_manager() {
    return static_cast<TestBrowserAutofillManager&>(
        driver_->GetAutofillManager());
  }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient client_;
  std::unique_ptr<TestAutofillDriver> driver_;
};

TEST_F(TabInteractionRecorderAndroidTest, HadFormInteraction) {
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  auto* helper = TabInteractionRecorderAndroid::FromWebContents(contents.get());

  EXPECT_FALSE(helper->has_form_interactions_in_session());
  EXPECT_EQ(nullptr, FormInteractionData::GetForCurrentDocument(
                   contents->GetPrimaryMainFrame()));
  OnTextFieldDidChangeForAutofillManager(autofill_manager());
  EXPECT_TRUE(helper->has_form_interactions_in_session());
  EXPECT_TRUE(FormInteractionData::GetForCurrentDocument(
                  contents->GetPrimaryMainFrame())
                  ->FormInteractionData::GetHasFormInteractionData());

  JNIEnv* env = base::android::AttachCurrentThread();
  EXPECT_TRUE(helper->HadFormInteractionInSession(env));
  EXPECT_TRUE(helper->HadFormInteractionInActivePage(env));
}

TEST_F(TabInteractionRecorderAndroidTest, HadFormInteractionThenNavigation) {
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  auto* helper = TabInteractionRecorderAndroid::FromWebContents(contents.get());

  EXPECT_FALSE(helper->has_form_interactions_in_session());
  EXPECT_EQ(nullptr, FormInteractionData::GetForCurrentDocument(
                         contents->GetPrimaryMainFrame()));
  OnTextFieldDidChangeForAutofillManager(autofill_manager());
  EXPECT_TRUE(helper->has_form_interactions_in_session());
  EXPECT_TRUE(FormInteractionData::GetForCurrentDocument(
                  contents->GetPrimaryMainFrame())
                  ->FormInteractionData::GetHasFormInteractionData());

  content::WebContentsTester::For(contents.get())
      ->NavigateAndCommit(GURL("https://bar.com"));
  task_environment()->RunUntilIdle();

  JNIEnv* env = base::android::AttachCurrentThread();
  EXPECT_TRUE(helper->HadFormInteractionInSession(env));
  EXPECT_FALSE(helper->HadFormInteractionInActivePage(env));
}

TEST_F(TabInteractionRecorderAndroidTest, HasNavigatedFromFirstPage) {
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  auto* helper = TabInteractionRecorderAndroid::FromWebContents(contents.get());

  EXPECT_FALSE(helper->HasNavigatedFromFirstPage());

  content::WebContentsTester::For(contents.get())
      ->NavigateAndCommit(GURL("https://bar.com"));
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(helper->HasNavigatedFromFirstPage());

  JNIEnv* env = base::android::AttachCurrentThread();
  // no navigation interaction if did not get user interaction.
  EXPECT_FALSE(helper->HadNavigationInteraction(env));
}

TEST_F(TabInteractionRecorderAndroidTest, DidGetUserInteraction) {
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  auto* helper = TabInteractionRecorderAndroid::FromWebContents(contents.get());

  EXPECT_FALSE(helper->did_get_user_interaction());
  helper->DidGetUserInteraction(blink::WebTouchEvent());
  EXPECT_TRUE(helper->did_get_user_interaction());

  JNIEnv* env = base::android::AttachCurrentThread();
  EXPECT_TRUE(helper->DidGetUserInteraction(env));
  // no navigation interaction if no navigation from first page is performed.
  EXPECT_FALSE(helper->HadNavigationInteraction(env));
}

TEST_F(TabInteractionRecorderAndroidTest,
       GetUserInteractionAndNavigateFromFirstPage) {
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  auto* helper = TabInteractionRecorderAndroid::FromWebContents(contents.get());

  helper->DidGetUserInteraction(blink::WebTouchEvent());
  content::WebContentsTester::For(contents.get())
      ->NavigateAndCommit(GURL("https://bar.com"));
  task_environment()->RunUntilIdle();

  JNIEnv* env = base::android::AttachCurrentThread();
  EXPECT_TRUE(helper->DidGetUserInteraction(env));
  EXPECT_TRUE(helper->HadNavigationInteraction(env));
}

TEST_F(TabInteractionRecorderAndroidTest, ResetInteractions) {
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  auto* helper = TabInteractionRecorderAndroid::FromWebContents(contents.get());

  // Simulate touch, text input, and navigation events.
  helper->DidGetUserInteraction(blink::WebTouchEvent());
  EXPECT_EQ(nullptr, FormInteractionData::GetForCurrentDocument(
                   contents->GetPrimaryMainFrame()));
  OnTextFieldDidChangeForAutofillManager(autofill_manager());
  EXPECT_TRUE(FormInteractionData::GetForCurrentDocument(
                  contents->GetPrimaryMainFrame())
                  ->FormInteractionData::GetHasFormInteractionData());

  content::WebContentsTester::For(contents.get())
      ->NavigateAndCommit(GURL("https://bar.com"));
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(helper->has_form_interactions_in_session());
  EXPECT_TRUE(helper->did_get_user_interaction());
  EXPECT_TRUE(helper->HasNavigatedFromFirstPage());

  // Assuming the record resets from Android.
  JNIEnv* env = base::android::AttachCurrentThread();
  helper->Reset(env);
  EXPECT_FALSE(helper->HadFormInteractionInSession(env));
  EXPECT_FALSE(helper->DidGetUserInteraction(env));
  EXPECT_FALSE(helper->HadNavigationInteraction(env));
  EXPECT_FALSE(helper->HadFormInteractionInActivePage(env));
}

// TODO(crbug.com/41496197): Re-enable this test.
TEST_F(TabInteractionRecorderAndroidTest, DISABLED_TestFormSeen) {
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  OnFormsSeenForAutofillManager(autofill_manager(),
                                contents->GetPrimaryMainFrame());

  EXPECT_NE(FormInteractionData::GetForCurrentDocument(
                contents->GetPrimaryMainFrame()),
            nullptr);
  EXPECT_FALSE(FormInteractionData::GetForCurrentDocument(
                   contents->GetPrimaryMainFrame())
                   ->GetHasFormInteractionData());
}

TEST_F(TabInteractionRecorderAndroidTest, TestFormSeenInDifferentFrame) {
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  OnFormsSeenForAutofillManager(autofill_manager(), nullptr);

  EXPECT_EQ(FormInteractionData::GetForCurrentDocument(
                contents->GetPrimaryMainFrame()),
            nullptr);
}
}  // namespace customtabs
