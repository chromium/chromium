// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_web_contents_destroyer.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_tab_modal_dialog_manager_delegate_android.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/window_container_type.mojom.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace tabs {

namespace {

class NavigationObserver : public content::WebContentsObserver {
 public:
  explicit NavigationObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents) {}

  void DidStartNavigation(content::NavigationHandle* handle) override {
    called_ = true;
  }

  bool called() const { return called_; }

 private:
  bool called_ = false;
};

}  // namespace

class TabWebContentsDestroyerTest : public ChromeRenderViewHostTestHarness {
 public:
  TabWebContentsDestroyerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~TabWebContentsDestroyerTest() override = default;

  TabWebContentsDestroyerTest(const TabWebContentsDestroyerTest&) = delete;
  TabWebContentsDestroyerTest& operator=(const TabWebContentsDestroyerTest&) =
      delete;
};

TEST_F(TabWebContentsDestroyerTest, NullWebContentsHandling) {
  // Passing null should be a safe no-op and not crash or allocate.
  TabWebContentsDestroyer::DestroyWebContents(nullptr);
}

TEST_F(TabWebContentsDestroyerTest, UnresponsiveRendererClosePageTimeout) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  content::WebContents* raw_web_contents = web_contents.get();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      raw_web_contents, GURL("https://example.com"));
  content::WebContentsDestroyedWatcher watcher(raw_web_contents);

  TabWebContentsDestroyer::DestroyWebContents(std::move(web_contents));
  EXPECT_FALSE(watcher.IsDestroyed());

  // 250ms is within the 500ms RenderFrameHostImpl unload timeout.
  task_environment()->FastForwardBy(base::Milliseconds(250));
  EXPECT_FALSE(watcher.IsDestroyed());

  // Fast forward past the 500ms timeout (total 600ms). The 500ms timer fires
  // CloseContents(), which posts Destroy().
  task_environment()->FastForwardBy(base::Milliseconds(350));
  EXPECT_TRUE(watcher.IsDestroyed());
}

TEST_F(TabWebContentsDestroyerTest, FallbackTimerExpiration) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  content::WebContents* raw_web_contents = web_contents.get();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      raw_web_contents, GURL("https://example.com"));
  content::WebContentsDestroyedWatcher watcher(raw_web_contents);

  TabWebContentsDestroyer::DestroyWebContents(std::move(web_contents));
  EXPECT_FALSE(watcher.IsDestroyed());

  // Disconnect the delegate so TabWebContentsDestroyer does not receive
  // CloseContents() from RenderFrameHostImpl's 500ms timeout. This isolates
  // and validates TabWebContentsDestroyer's 2-second fallback safety net.
  raw_web_contents->SetDelegate(nullptr);

  // Fast forward by 1 second (past the 500ms RFH timeout). The 2-second
  // TabWebContentsDestroyer fallback timer has not fired yet.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(watcher.IsDestroyed());

  // Fast forward by another second (total 2 seconds). The fallback timer fires.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(watcher.IsDestroyed());
}

TEST_F(TabWebContentsDestroyerTest, CloseContentsTriggersDeferredDestruction) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  content::WebContents* raw_web_contents = web_contents.get();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      raw_web_contents, GURL("https://example.com"));
  content::WebContentsDestroyedWatcher watcher(raw_web_contents);

  TabWebContentsDestroyer::DestroyWebContents(std::move(web_contents));
  EXPECT_FALSE(watcher.IsDestroyed());

  // Renderer responds to ClosePage or triggers window closure via
  // CloseContents.
  raw_web_contents->GetDelegate()->CloseContents(raw_web_contents);
  EXPECT_FALSE(watcher.IsDestroyed());

  // Running posted tasks triggers destruction without waiting for the 2-second
  // timer.
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(watcher.IsDestroyed());
}

TEST_F(TabWebContentsDestroyerTest, RenderProcessCrash) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  content::WebContents* raw_web_contents = web_contents.get();
  content::WebContentsDestroyedWatcher watcher(raw_web_contents);

  TabWebContentsDestroyer::DestroyWebContents(std::move(web_contents));
  EXPECT_FALSE(watcher.IsDestroyed());

  // Simulating render process crash should immediately trigger
  // PrimaryMainFrameRenderProcessGone.
  auto* rph = static_cast<content::MockRenderProcessHost*>(
      raw_web_contents->GetPrimaryMainFrame()->GetProcess());
  rph->SimulateCrash();

  EXPECT_TRUE(watcher.IsDestroyed());
}

TEST_F(TabWebContentsDestroyerTest, ProfileDestruction) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(otr_profile));
  content::WebContents* raw_web_contents = web_contents.get();
  content::WebContentsDestroyedWatcher watcher(raw_web_contents);

  TabWebContentsDestroyer::DestroyWebContents(std::move(web_contents));
  EXPECT_FALSE(watcher.IsDestroyed());

  // Destroying the observed profile should immediately destroy the WebContents.
  profile()->DestroyOffTheRecordProfile(otr_profile);
  EXPECT_TRUE(watcher.IsDestroyed());
}

TEST_F(TabWebContentsDestroyerTest, DialogSuppressionAndManager) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  content::WebContents* raw_web_contents = web_contents.get();

  javascript_dialogs::TabModalDialogManager::CreateForWebContents(
      raw_web_contents,
      std::make_unique<JavaScriptTabModalDialogManagerDelegateAndroid>(
          raw_web_contents));
  auto* dialog_manager =
      javascript_dialogs::TabModalDialogManager::FromWebContents(
          raw_web_contents);
  ASSERT_NE(nullptr, dialog_manager);

  bool did_suppress = false;
  dialog_manager->RunJavaScriptDialog(
      raw_web_contents, raw_web_contents->GetPrimaryMainFrame(),
      content::JavaScriptDialogType::JAVASCRIPT_DIALOG_TYPE_ALERT,
      u"Test alert", u"",
      base::BindOnce([](bool accept, const std::u16string& user_input) {}),
      &did_suppress);

  content::WebContentsDestroyedWatcher watcher(raw_web_contents);
  TabWebContentsDestroyer::DestroyWebContents(std::move(web_contents));

  content::WebContentsDelegate* delegate = raw_web_contents->GetDelegate();
  ASSERT_NE(nullptr, delegate);

  // Dialogs should be suppressed.
  EXPECT_TRUE(delegate->ShouldSuppressDialogs(raw_web_contents));

  // GetJavaScriptDialogManager should return the TabModalDialogManager.
  EXPECT_EQ(dialog_manager,
            delegate->GetJavaScriptDialogManager(raw_web_contents));

  // Advance time to allow destruction to complete cleanly and cancel dialogs.
  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(watcher.IsDestroyed());
}

TEST_F(TabWebContentsDestroyerTest, NavigationSuppressionAndInterception) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  content::WebContents* raw_web_contents = web_contents.get();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      raw_web_contents, GURL("https://example.com"));
  content::WebContentsDestroyedWatcher watcher(raw_web_contents);

  TabWebContentsDestroyer::DestroyWebContents(std::move(web_contents));

  content::WebContentsDelegate* delegate = raw_web_contents->GetDelegate();
  ASSERT_NE(nullptr, delegate);

  // OpenURLFromTab should be suppressed (return nullptr).
  content::OpenURLParams params(
      GURL("https://example.com"), content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_LINK,
      /*is_renderer_initiated=*/true);
  EXPECT_EQ(nullptr, delegate->OpenURLFromTab(raw_web_contents, params,
                                              base::DoNothing()));

  // DidStartNavigation observer safety and stopping navigation.
  NavigationObserver observer(raw_web_contents);

  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com"), raw_web_contents->GetPrimaryMainFrame());
  EXPECT_TRUE(observer.called());

  task_environment()->RunUntilIdle();
  EXPECT_FALSE(raw_web_contents->IsLoading());

  // Clean up via fast forwarding timer.
  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(watcher.IsDestroyed());
}

TEST_F(TabWebContentsDestroyerTest, WindowCreationSuppression) {
  std::unique_ptr<content::WebContents> web_contents = CreateTestWebContents();
  content::WebContents* raw_web_contents = web_contents.get();

  TabWebContentsDestroyer::DestroyWebContents(std::move(web_contents));

  content::WebContentsDelegate* delegate = raw_web_contents->GetDelegate();
  ASSERT_NE(nullptr, delegate);

  // Intercept window/popup creation requests.
  EXPECT_TRUE(delegate->IsWebContentsCreationOverridden(
      raw_web_contents->GetPrimaryMainFrame(),
      raw_web_contents->GetPrimaryMainFrame()->GetSiteInstance(),
      content::mojom::WindowContainerType::NORMAL, GURL("https://example.com"),
      "test_frame", GURL("https://target.example.com")));

  // Reject creation of new WebContents/popups.
  blink::mojom::WindowFeatures window_features;
  content::StoragePartitionConfig partition_config =
      content::StoragePartitionConfig::CreateDefault(profile());
  EXPECT_EQ(nullptr,
            delegate->CreateCustomWebContents(
                raw_web_contents->GetPrimaryMainFrame(),
                raw_web_contents->GetPrimaryMainFrame()->GetSiteInstance(),
                /*is_new_browsing_instance=*/true, GURL("https://example.com"),
                "test_frame", GURL("https://target.example.com"),
                WindowOpenDisposition::NEW_POPUP, window_features,
                partition_config, /*session_storage_namespace=*/nullptr));

  task_environment()->FastForwardBy(base::Seconds(2));
}

}  // namespace tabs
