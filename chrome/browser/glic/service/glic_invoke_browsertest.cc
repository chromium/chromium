// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_invoke_handler.h"
#include "chrome/browser/glic/service/metrics/glic_instance_helper_metrics.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_histogram_tester.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/ui_test_utils.h"
#endif

namespace glic {

class GlicInvokeBrowserTest : public GlicBrowserTestMixin<PlatformBrowserTest> {
 public:
  GlicInvokeBrowserTest() {
    feature_list_.InitAndDisableFeature(
        features::kGlicDefaultToLastActiveConversation);
  }
  ~GlicInvokeBrowserTest() override = default;

 protected:
  static InvokeWithAutoSubmitPasskey GetPassKey() {
    return InvokeWithAutoSubmitPasskeyProvider::GetPassKey();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeWithInvalidTab) {
  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.target.surface = tabs::TabHandle::Null();
  options.on_error = error_future.GetCallback();

  coordinator().Invoke(std::move(options));

  EXPECT_EQ(error_future.Get(), GlicInvokeError::kInvalidTab);
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeWithTabDestroyedBeforeInvoke) {
  // 1. Create a new tab and get its handle.
  tabs::TabInterface* tab = CreateUserInitiatedTab(GURL("about:blank"));
  tabs::TabHandle handle = tab->GetHandle();
  ASSERT_TRUE(handle.Get());

  // 2. Close/destroy the tab.
  tab->Close();
  ASSERT_FALSE(handle.Get());

  // 3. Try to invoke Glic targeting the destroyed tab.
  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.target.surface = handle;
  options.on_error = error_future.GetCallback();

  coordinator().Invoke(std::move(options));

  // 4. It should fail with GlicInvokeError::kTabClosed because the handle is
  // invalid now.
  EXPECT_EQ(error_future.Get(), GlicInvokeError::kTabClosed);
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeWithEmptyConversationId) {
  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(glic::Target(glic::ConversationId("")),
                            mojom::InvocationSource::kOsButton);
  options.on_error = error_future.GetCallback();
  options.target.surface = DefaultSurface{
      GetTabListInterface()->GetActiveTab()->GetBrowserWindowInterface()};

  coordinator().Invoke(std::move(options));

  EXPECT_EQ(error_future.Get(), GlicInvokeError::kInvalidConversationId);
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeWhenWebClientAlreadySet) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();

  // Open Glic to set it up.
  ASSERT_OK(OpenGlicForActiveTab());

  auto* instance = GetInstanceForTab(tab);

  // Wait until setup is complete
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return instance->host().IsWebClientConnected(); }));

  // Now, invoke should hit the fast path.
  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();

  coordinator().Invoke(std::move(options));

  // The success callback should be called relatively quickly via fast-pathing
  // through IsReady(), without waiting for WebClientConnected. However, it is
  // still asynchronous due to the Mojo IPC, so we must Wait().
  EXPECT_TRUE(success_future.Wait());
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeBeforeWebClientSet) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();

  // Call invoke. This will create the instance and wait for WebClientSet.
  coordinator().Invoke(std::move(options));

  // The success callback should be called after observing WebClientSet.
  EXPECT_TRUE(success_future.Wait());
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeCallsOnClientConnected) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  base::test::TestFuture<void> success_future;
  base::test::TestFuture<base::WeakPtr<GlicInstance>> connected_future;

  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();
  options.on_client_connected = connected_future.GetCallback();

  // Verify there is no connected web client before starting.
  auto* instance_before = GetInstanceForTab(tab);
  EXPECT_TRUE(!instance_before ||
              !instance_before->host().IsWebClientConnected());

  // Call invoke. This will create the instance and wait for WebClientSet.
  coordinator().Invoke(std::move(options));

  // The connected callback should be called after observing WebClientSet.
  EXPECT_TRUE(connected_future.Wait());

  // Verify that there is a connected web client when our callback fires.
  base::WeakPtr<GlicInstance> instance = connected_future.Get();
  ASSERT_TRUE(instance);
  EXPECT_TRUE(instance->host().IsWebClientConnected());

  // Verify that the passed instance is the correct one.
  EXPECT_EQ(instance.get(), GetInstanceForTab(tab));

  // The success callback should be called after full completion.
  EXPECT_TRUE(success_future.Wait());
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeCallsOnConversationIdReady) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  base::test::TestFuture<void> success_future;
  base::test::TestFuture<std::string> conversation_id_future;

  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();

  GlicInvokeWithAutoSubmitOptions auto_submit_options;
  auto_submit_options.on_conversation_id_ready =
      conversation_id_future.GetCallback();

  // Call invoke with auto-submit.
  coordinator().InvokeWithAutoSubmit(GetPassKey(), std::move(options),
                                     std::move(auto_submit_options));

  // Simulate the client registering a conversation.
  GlicInstanceImpl* instance = coordinator().GetInstanceImplForTab(tab);
  ASSERT_TRUE(instance);

  const std::string expected_conversation_id = "test_conversation_id";
  auto info = mojom::ConversationInfo::New();
  info->conversation_id = expected_conversation_id;
  instance->RegisterConversation(std::move(info), base::DoNothing());

  // The callback should be called with the correct conversation ID.
  EXPECT_EQ(conversation_id_future.Get(), expected_conversation_id);

  // The success callback should be called after full completion.
  EXPECT_TRUE(success_future.Wait());
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeWithAutoSubmitHidden) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  base::test::TestFuture<void> success_future;
  base::test::TestFuture<std::string> conversation_id_future;

  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();

  GlicInvokeWithAutoSubmitOptions auto_submit_options;
  auto_submit_options.on_conversation_id_ready =
      conversation_id_future.GetCallback();
  auto_submit_options.show_panel = false;

  // Call invoke with auto-submit in background (hidden).
  coordinator().InvokeWithAutoSubmit(GetPassKey(), std::move(options),
                                     std::move(auto_submit_options));

  GlicInstanceImpl* instance = coordinator().GetInstanceImplForTab(tab);
  ASSERT_TRUE(instance);

  // Simulate the client registering a conversation (which completes the flow).
  const std::string expected_conversation_id = "test_conversation_id";
  auto info = mojom::ConversationInfo::New();
  info->conversation_id = expected_conversation_id;
  instance->RegisterConversation(std::move(info), base::DoNothing());

  // The conversation ID should be passed to our callback.
  EXPECT_EQ(conversation_id_future.Get(), expected_conversation_id);

  // The success callback should be called after full completion.
  EXPECT_TRUE(success_future.Wait());

  // The instance should be connected.
  EXPECT_TRUE(instance->host().IsWebClientConnected());

  // BUT the panel must NOT be showing and its visibility must be HIDDEN.
  EXPECT_FALSE(instance->IsShowing());
  EXPECT_OK(
      WaitForWebUiContentsVisibility(instance, content::Visibility::HIDDEN));
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeWithWaitForPanelOpen) {
  // Create a tab with a loaded page to measure width.
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());

#if !BUILDFLAG(IS_ANDROID)
  // Measure page width before invocation.
  int width_before =
      content::EvalJs(tab->GetContents(), "window.innerWidth").ExtractInt();
#endif  // !BUILDFLAG(IS_ANDROID)

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.wait_for_panel_open = true;
  options.on_success = success_future.GetCallback();

  coordinator().Invoke(std::move(options));

  auto* instance = GetInstanceForTab(tab);
  ASSERT_TRUE(instance);

  // The success callback should be called after the panel is showing and
  // stabilized.
  EXPECT_TRUE(success_future.Wait());
  EXPECT_TRUE(instance->IsShowing());

#if !BUILDFLAG(IS_ANDROID)
  // Measure page width after invocation. It should be smaller because the
  // side panel takes up space.
  int width_after =
      content::EvalJs(tab->GetContents(), "window.innerWidth").ExtractInt();
  EXPECT_LT(width_after, width_before);
#endif  // !BUILDFLAG(IS_ANDROID)
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeWhileInvokeInProgress) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  base::test::TestFuture<GlicInvokeError> error_future1;
  GlicInvokeOptions options1(glic::Target(*tab),
                             mojom::InvocationSource::kOsButton);

  coordinator().Invoke(std::move(options1));

  base::test::TestFuture<GlicInvokeError> error_future2;
  GlicInvokeOptions options2(glic::Target(*tab),
                             mojom::InvocationSource::kOsButton);
  options2.on_error = error_future2.GetCallback();

  // Try to invoke again while the first one is still in progress for the same
  // instance.
  coordinator().Invoke(std::move(options2));

  // The second invoke should fail synchronously.
  EXPECT_EQ(error_future2.Get(), GlicInvokeError::kInvokeInProgress);
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeTimeoutBehaviors) {
  // 1. Test custom short timeout
  base::test::TestFuture<GlicInvokeError> short_error_future;
  GlicInvokeOptions short_options(mojom::InvocationSource::kOsButton);
  short_options.on_error = short_error_future.GetCallback();
  short_options.timeout = base::Milliseconds(1);
  short_options.target.surface = DefaultSurface{
      GetTabListInterface()->GetActiveTab()->GetBrowserWindowInterface()};

  coordinator().Invoke(std::move(short_options));

  // The first invoke should time out quickly.
  EXPECT_EQ(short_error_future.Get(), GlicInvokeError::kTimeout);

  // 2. Test that a longer timeout actually takes longer, ensuring the
  // specified duration isn't being ignored resulting in an instant timeout.
  base::test::TestFuture<GlicInvokeError> long_error_future;
  GlicInvokeOptions long_options(mojom::InvocationSource::kOsButton);
  long_options.on_error = long_error_future.GetCallback();
  long_options.timeout = base::Milliseconds(100);
  long_options.target.surface = DefaultSurface{
      GetTabListInterface()->GetActiveTab()->GetBrowserWindowInterface()};

  base::ElapsedTimer elapsed_timer;
  coordinator().Invoke(std::move(long_options));

  // Wait for the timeout to occur.
  EXPECT_EQ(long_error_future.Get(), GlicInvokeError::kTimeout);

  // Verify it took at least some fraction of the longer timeout, proving
  // it didn't instantly time out like the short one.
  EXPECT_GE(elapsed_timer.Elapsed(), base::Milliseconds(50));
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeFailsOnInstanceDestruction) {
  // Add a new tab so we don't close the browser when we close the active tab.
  CreateAndActivateTab(GURL("about:blank"));

  // Go back to the original tab and open Glic.
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  ActivateTab(tab1);

  ASSERT_OK(OpenGlicForActiveTab());

  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(glic::Target(*tab1),
                            mojom::InvocationSource::kOsButton);
  options.on_error = error_future.GetCallback();

  coordinator().Invoke(std::move(options));

  // Destroy the instance while Invoke is in progress by closing the tab it is
  // bound to.
  tab1->Close();

  // The error should be either kInstanceDestroyed or kTabClosed, depending on
  // the order of destruction. The user expects it to cause instance deletion.
  EXPECT_EQ(error_future.Get(), GlicInvokeError::kTabClosed);
}

class GlicInvokeNonConnectingBrowserTest : public GlicInvokeBrowserTest {
 public:
  GlicInvokeNonConnectingBrowserTest() {
    SetGlicPagePath("/non_existent.html");
  }
};

IN_PROC_BROWSER_TEST_F(GlicInvokeNonConnectingBrowserTest,
                       InvokeWithTabClosedSurvivingInstance) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));

  // Go back to tab1 to invoke on it.
  ActivateTab(tab1);

  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(glic::Target(*tab1),
                            mojom::InvocationSource::kOsButton);
  options.on_error = error_future.GetCallback();

  coordinator().Invoke(std::move(options));

  GlicInstanceImpl* instance = GetInstanceForTab(tab1);
  ASSERT_TRUE(instance);

  // Associate tab2 with the same instance to keep it alive when tab1 closes.
  coordinator().ShowInstanceForTabs({tab2}, instance->id());

  // Close tab1 while Invoke is in progress.
  tab1->Close();

  // The error should be kTabClosed because the instance survives (due to tab2).
  EXPECT_EQ(error_future.Get(), GlicInvokeError::kTabClosed);

  // Flush the message loop to ensure cleanup tasks complete.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeSuccess) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();

  coordinator().Invoke(std::move(options));

  EXPECT_TRUE(success_future.Wait());
  EXPECT_TRUE(GetInstanceForTab(tab));
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeWithClipboardPolicySuccess) {
  tabs::TabInterface* tab = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(content::NavigateToURL(tab->GetContents(), GURL("about:blank")));

  // Create mock AdditionalContext containing PNG image data.
  auto context_mojom = mojom::AdditionalContext::New();
  context_mojom->source = mojom::AdditionalContextSource::kShareContextMenu;
  context_mojom->name = "https://example.com/image.png";

  auto context_data = mojom::ContextData::New();
  context_data->mime_type = "image/png";
  // The first 4 bytes of a valid PNG file header, so it isn't rejected.
  context_data->data =
      mojo_base::BigBuffer(std::vector<uint8_t>{0x89, 0x50, 0x4E, 0x47});

  context_mojom->parts.push_back(
      mojom::AdditionalContextPart::NewData(std::move(context_data)));

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();

  content::RenderFrameHost* rfh = tab->GetContents()->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh);

  options.additional_context = AdditionalTabContext(
      std::move(context_mojom), rfh->GetGlobalId(), PolicyCheck::kClipboard);

  coordinator().Invoke(std::move(options));

  EXPECT_TRUE(success_future.Wait());
  EXPECT_TRUE(GetInstanceForTab(tab));
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeWithPolicyCheckNone) {
  tabs::TabInterface* tab = CreateAndActivateTab(GURL("about:blank"));

  // Create mock AdditionalContext containing PNG image data.
  auto context_mojom = mojom::AdditionalContext::New();
  context_mojom->source = mojom::AdditionalContextSource::kShareContextMenu;
  context_mojom->name = "https://example.com/image.png";

  auto context_data = mojom::ContextData::New();
  context_data->mime_type = "image/png";
  // The first 4 bytes of a valid PNG file header, so it isn't rejected.
  context_data->data =
      mojo_base::BigBuffer(std::vector<uint8_t>{0x89, 0x50, 0x4E, 0x47});

  context_mojom->parts.push_back(
      mojom::AdditionalContextPart::NewData(std::move(context_data)));

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(tab),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();

  // Supply the additional context and omit the source frame ID, but specify
  // PolicyCheck::kNone. This should bypass the validation tasks and succeed.
  options.additional_context = AdditionalTabContext(
      std::move(context_mojom), content::GlobalRenderFrameHostId(),
      PolicyCheck::kNone);

  coordinator().Invoke(std::move(options));

  EXPECT_TRUE(success_future.Wait());
  EXPECT_TRUE(GetInstanceForTab(tab));
}
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeWithTabsToPin) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  tabs::TabInterface* tab2 = CreateUserInitiatedTab(GURL("about:blank"));
  // Ensure tab1 is active for Glic invocation.
  ActivateTab(tab1);

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab1),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();
  options.tab_sharing.tabs_to_pin = {tab2->GetHandle()};
  options.tab_sharing.pin_trigger = GlicPinTrigger::kInstanceCreation;

  coordinator().Invoke(std::move(options));

  EXPECT_TRUE(success_future.Wait());

  auto* instance = GetInstanceForTab(tab1);
  ASSERT_TRUE(instance);

  // Verify that tab2 was pinned.
  auto usage = instance->sharing_manager().GetPinnedTabUsage(tab2->GetHandle());
  ASSERT_TRUE(usage.has_value());
  EXPECT_EQ(usage->pin_event.trigger, GlicPinTrigger::kInstanceCreation);
}

// This test is disabled on Android because incognito window creation
// behavior differs and is not supported by this test setup.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       ResolveTargetSurfaceCreatesNewWindow) {
  Profile* incognito_profile =
      GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  // Initially there should be no browsers for incognito profile.
  EXPECT_EQ(ProfileBrowserCollection::GetForProfile(incognito_profile)
                ->GetLastActiveBrowser(),
            nullptr);

  BrowserWindowInterface* new_browser = nullptr;
  {
    // Call ResolveTargetSurface with incognito profile.
    GlicInvokeHandler::ResolvedTarget resolved =
        GlicInvokeHandler::ResolveTargetSurface(incognito_profile,
                                                glic::Target{});

    EXPECT_TRUE(resolved.is_new);
    ASSERT_TRUE(resolved.tab);

    // Verify it created an incognito browser.
    new_browser = ProfileBrowserCollection::GetForProfile(incognito_profile)
                      ->GetLastActiveBrowser();
    ASSERT_TRUE(new_browser);
    EXPECT_TRUE(new_browser->GetProfile()->IsOffTheRecord());
  }

  // Clean up the new window.
  CloseBrowserSynchronously(new_browser);
}
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/504753617): Re-enable the test.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_InvokeWithNewTab DISABLED_InvokeWithNewTab
#else
#define MAYBE_InvokeWithNewTab InvokeWithNewTab
#endif
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, MAYBE_InvokeWithNewTab) {
  BrowserWindowInterface* browser_window =
      GetTabListInterface()->GetActiveTab()->GetBrowserWindowInterface();
  int tab_count_before = GetTabListInterface()->GetTabCount();

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(browser_window),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();

  GlicTestTabAddedWaiter waiter(GetProfile());

  coordinator().Invoke(std::move(options));

  tabs::TabInterface* new_tab = waiter.Wait();
  ASSERT_TRUE(new_tab);

  EXPECT_TRUE(success_future.Wait());

  // Verify a new tab was added.
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), tab_count_before + 1);

  // Verify the active tab is the new one.
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(active_tab);
  EXPECT_EQ(active_tab, new_tab);

  // Verify it is not loading now.
  // TODO(crbug.com/503876352): assert that tab navigation has completed.
  // EXPECT_FALSE(active_tab->GetContents()->IsLoading());

  // Verify instance exists for the new tab.
  EXPECT_TRUE(GetInstanceForTab(active_tab));
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_InvokeWithNewTabBackground DISABLED_InvokeWithNewTabBackground
#else
#define MAYBE_InvokeWithNewTabBackground InvokeWithNewTabBackground
#endif
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       MAYBE_InvokeWithNewTabBackground) {
  BrowserWindowInterface* browser_window =
      GetTabListInterface()->GetActiveTab()->GetBrowserWindowInterface();
  int tab_count_before = GetTabListInterface()->GetTabCount();
  tabs::TabInterface* active_tab_before = GetTabListInterface()->GetActiveTab();

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(
      glic::Target(glic::NewTab{browser_window, /*open_in_foreground=*/false}),
      mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();

  GlicTestTabAddedWaiter waiter(GetProfile());

  coordinator().Invoke(std::move(options));

  tabs::TabInterface* new_tab = waiter.Wait();
  ASSERT_TRUE(new_tab);

  EXPECT_TRUE(success_future.Wait());

  // Verify a new tab was added.
  EXPECT_EQ(GetTabListInterface()->GetTabCount(), tab_count_before + 1);

  // Verify the active tab is STILL the old one.
  tabs::TabInterface* active_tab_after = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(active_tab_after);
  EXPECT_EQ(active_tab_after, active_tab_before);
  EXPECT_NE(active_tab_after, new_tab);

  // Verify instance exists for the new tab.
  EXPECT_TRUE(GetInstanceForTab(new_tab));
}

// This test is disabled on Android because creating a new window behavior
// differs and is not supported by this test setup.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeWithNewTabCreatesNewWindow) {
  size_t browser_count_before =
      GlobalBrowserCollection::GetInstance()->GetSize();

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(glic::NewTab{}),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();

  coordinator().Invoke(std::move(options));

  EXPECT_TRUE(success_future.Wait());

  // Verify a new browser window was created.
  EXPECT_EQ(GlobalBrowserCollection::GetInstance()->GetSize(),
            browser_count_before + 1);
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeWithAutoSubmitSuccess) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();

  coordinator().InvokeWithAutoSubmit(GetPassKey(), std::move(options));

  EXPECT_TRUE(success_future.Wait());
  EXPECT_TRUE(GetInstanceForTab(tab));
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeWaitsForFreCompletion_Arm2) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  SetFRECompletion(GetProfile(), prefs::FreStatus::kNotStarted);

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(Target(*tab), mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();

  coordinator().Invoke(std::move(options));

  // The success callback should NOT be called yet because FRE is not completed.
  EXPECT_FALSE(success_future.IsReady());

  // Complete FRE.
  SetFRECompletion(GetProfile(), prefs::FreStatus::kCompleted);

  // Now the success callback should be called.
  EXPECT_TRUE(success_future.Wait());
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeWaitsForFreCompletion_Override) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  SetFRECompletion(GetProfile(), prefs::FreStatus::kNotStarted);

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(Target(*tab), mojom::InvocationSource::kOsButton);
  options.fre_override = mojom::FreOverride::kTrustFirstClick;
  options.on_success = success_future.GetCallback();

  coordinator().Invoke(std::move(options));

  // The success callback should NOT be called yet because FRE is not completed.
  EXPECT_FALSE(success_future.IsReady());

  // Complete FRE.
  SetFRECompletion(GetProfile(), prefs::FreStatus::kCompleted);

  // Now the success callback should be called.
  EXPECT_TRUE(success_future.Wait());
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeDoesNotWaitForFreCompletion_TrustFirstInline) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  SetFRECompletion(GetProfile(), prefs::FreStatus::kNotStarted);

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.fre_override = mojom::FreOverride::kTrustFirstInline;
  options.on_success = success_future.GetCallback();
  options.target = Target(*tab);

  coordinator().Invoke(std::move(options));

  // The invocation should complete successfully without waiting for FRE
  // completion (which remains not started). We still need to Wait() for the
  // async Mojo operations to complete.
  EXPECT_TRUE(success_future.Wait());
}

class GlicInvokeActuationBrowserTest : public GlicInvokeBrowserTest {
 public:
  GlicInvokeActuationBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        ::features::kGlicActor,
        {{::features::kGlicActorPolicyControlExemption.name, "true"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInvokeActuationBrowserTest,
                       InvokeWithActuationFeatureMode) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.feature_mode = mojom::FeatureMode::kActuation;
  options.on_success = success_future.GetCallback();
  options.target = Target(*tab);

  // 1. Trigger invocation in actuation mode.
  coordinator().Invoke(std::move(options));

  auto* instance = GetInstanceForTab(tab);
  ASSERT_TRUE(instance);

  // Wait until the web client is connected to ensure that setup has completed
  // and we have transitioned to waiting for actuation.
  ASSERT_OK(WaitForGlicClient(instance));

  // The invocation should NOT complete yet because it is waiting for actuation.
  EXPECT_FALSE(success_future.IsReady());

  // 2. Simulate actuation starting by creating an actor task.
  auto task_id_result = CreateActorTask(instance);
  ASSERT_TRUE(task_id_result.has_value()) << task_id_result.error();
  auto task_id = task_id_result.value();

  // Verify the instance transitioned to actuating.
  EXPECT_TRUE(instance->IsActuating());

  // Spin the message loop to ensure any incorrect completion tasks run.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // The invocation should STILL not be complete because actuation is ongoing.
  EXPECT_FALSE(success_future.IsReady());

  // 3. Simulate actuation finishing by stopping the task.
  instance->GetActorTaskManager()->GetClientSessionForTesting()->StopActorTask(
      static_cast<int32_t>(task_id), mojom::ActorTaskStopReason::kTaskComplete);

  // Verify the instance is no longer actuating.
  EXPECT_FALSE(instance->IsActuating());

  // Now the invocation should finally complete.
  EXPECT_TRUE(success_future.Wait());
}

class GlicInvokeDefaultToLastActiveBrowserTest : public GlicInvokeBrowserTest {
 public:
  GlicInvokeDefaultToLastActiveBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kGlicDefaultToLastActiveConversation);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInvokeDefaultToLastActiveBrowserTest,
                       NewTabDefaultsToLastActiveIfEnabled) {
  base::UserActionTester user_action_tester;
  GlicHistogramTester histogram_tester;

  ASSERT_OK_AND_ASSIGN(auto instance1, OpenGlicForActiveTab());

  PreventDeletionOnClose(instance1, "test_conversation_1");

  // Close the side panel on tab 1 to prevent new tab daisy chaining.
  ASSERT_OK(CloseGlicForTabAndWait(GetTabListInterface()->GetActiveTab()));

  // Switch to Tab 2
  CreateAndActivateTab(GURL("about:blank"));

  // Open Glic for Tab 2
  ASSERT_OK_AND_ASSIGN(auto instance2, OpenGlicForActiveTab());

  // With the feature enabled, the same instance should be reused since it was
  // the last active and the recency limit was less than 20 minutes (default).
  EXPECT_EQ(instance1, instance2);

  // Verify the metric was logged.
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Glic.Instance.DaisyChain.LastActiveInstance.Success"),
            1);

  histogram_tester.ExpectTotalCount(
      "Glic.Instance.TimeSinceLastInstanceActiveOnOpen", 1);

  // Simulate user input to trigger first action metric.
  instance2->instance_metrics().OnUserInputSubmitted(
      mojom::WebClientMode::kText);

  histogram_tester.ExpectUniqueSample(
      "Glic.Instance.AutoOpenedPanel.FirstAction.LastActiveInstance",
      DaisyChainFirstAction::kInputSubmitted, 1);
}

class GlicInvokeDefaultToLastActiveActuatingBrowserTest
    : public GlicInvokeDefaultToLastActiveBrowserTest {
 public:
  GlicInvokeDefaultToLastActiveActuatingBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        ::features::kGlicActor,
        {{::features::kGlicActorPolicyControlExemption.name, "true"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInvokeDefaultToLastActiveActuatingBrowserTest,
                       NewTabDoesNotDefaultToLastActiveIfActuating) {
  GlicHistogramTester histogram_tester;
  ASSERT_OK_AND_ASSIGN(auto instance1, OpenGlicForActiveTab());

  PreventDeletionOnClose(instance1, "test_conversation_1");

  // Wait for the instance to be ready so we can create a task.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return instance1->host().GetPrimaryWebUiState() ==
               glic::mojom::WebUiState::kReady &&
           instance1->host().GetPrimaryWebClient();
  }));

  // Create a task to make it "actuating".
  ASSERT_OK(CreateActorTask(instance1));
  EXPECT_TRUE(instance1->IsActuating());

  // Close the side panel on tab 1 to prevent new tab daisy chaining.
  ASSERT_OK(CloseGlicForTabAndWait(GetTabListInterface()->GetActiveTab()));

  // Switch to Tab 2
  CreateAndActivateTab(GURL("about:blank"));

  // Open Glic for Tab 2
  ASSERT_OK_AND_ASSIGN(auto instance2, OpenGlicForActiveTab());

  // Since instance1 was actuating, it should NOT be reused.
  EXPECT_NE(instance1, instance2);

  // Verify that no metric was logged since we did not default to last active.
  histogram_tester.ExpectTotalCount(
      "Glic.Instance.AutoOpenedPanel.FirstAction.LastActiveInstance", 0);
}

class GlicInvokeDefaultToLastActiveExpiredBrowserTest
    : public GlicInvokeBrowserTest {
 public:
  GlicInvokeDefaultToLastActiveExpiredBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicDefaultToLastActiveConversation,
        {{features::kGlicDefaultToLastActiveConversationMaxRecency.name,
          "0m"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInvokeDefaultToLastActiveExpiredBrowserTest,
                       NewTabDoesNotDefaultToLastActiveIfExpired) {
  GlicHistogramTester histogram_tester;
  ASSERT_OK_AND_ASSIGN(auto instance1, OpenGlicForActiveTab());

  PreventDeletionOnClose(instance1, "test_conversation_2");

  // Close the side panel on tab 1 to prevent new tab daisy chaining.
  ASSERT_OK(CloseGlicForTabAndWait(GetTabListInterface()->GetActiveTab()));

  // Switch to Tab 2
  CreateAndActivateTab(GURL("about:blank"));

  // Open Glic for Tab 2
  ASSERT_OK_AND_ASSIGN(auto instance2, OpenGlicForActiveTab());

  // With the parameter set to 0m, the recency limit should be hit immediately,
  // causing a new instance to be created instead of reusing the old one.
  EXPECT_NE(instance1, instance2);

  // Verify that no metric was logged since we did not default to last active.
  histogram_tester.ExpectTotalCount(
      "Glic.Instance.AutoOpenedPanel.FirstAction.LastActiveInstance", 0);

  histogram_tester.ExpectTotalCount(
      "Glic.Instance.TimeSinceLastInstanceActiveOnOpen", 1);
}

}  // namespace glic
