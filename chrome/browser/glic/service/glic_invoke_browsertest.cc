// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_contents_manager.h"
#include "chrome/browser/glic/public/glic_context_menu_invocation_helper.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/glic_invoke_handler.h"
#include "chrome/browser/glic/service/glic_ui_types.h"
#include "chrome/browser/glic/service/metrics/glic_instance_helper_metrics.h"
#include "chrome/browser/glic/service/metrics/glic_invoke_metrics.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_histogram_tester.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/common/webui_url_constants.h"
#include "components/enterprise/data_controls/core/browser/prefs.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/ui_test_utils.h"
#endif

namespace glic {

namespace {

mojom::AdditionalContextPtr CreateMockAdditionalContext(
    const std::string& mime_type = "image/png",
    const std::vector<uint8_t>& data = {0x89, 0x50, 0x4E, 0x47}) {
  auto context_mojom = mojom::AdditionalContext::New();
  context_mojom->source = mojom::AdditionalContextSource::kShareContextMenu;
  context_mojom->name = "https://example.com/image.png";

  auto context_data = mojom::ContextData::New();
  context_data->mime_type = mime_type;
  context_data->data = mojo_base::BigBuffer(data);

  context_mojom->parts.push_back(
      mojom::AdditionalContextPart::NewData(std::move(context_data)));

  return context_mojom;
}

}  // namespace

class GlicInvokeBrowserTest : public GlicBrowserTestMixin<PlatformBrowserTest> {
 public:
  GlicInvokeBrowserTest() = default;
  ~GlicInvokeBrowserTest() override = default;

 protected:
  static InvokeWithAutoSubmitPasskey GetPassKey() {
    return InvokeWithAutoSubmitPasskeyProvider::GetPassKey();
  }

  // Puts `instance` into live (audio) mode, as the web client would.
  [[nodiscard]] TestResult<> EnterLiveMode(GlicInstanceImpl* instance) {
    RETURN_IF_ERROR(WaitForGlicClient(instance));
    instance->OnInteractionModeChange(mojom::WebClientMode::kAudio);
    if (!instance->IsLiveMode()) {
      return base::unexpected("EnterLiveMode: instance is not in live mode");
    }
    return base::ok();
  }

  // Opens Glic for the active tab, detaches it into the floating panel, and
  // puts it into live mode. The active tab stays bound to the instance.
  [[nodiscard]] TestResult<GlicInstanceImpl*> OpenLiveModeFloatyForActiveTab() {
    ASSIGN_OR_RETURN(GlicInstanceImpl * instance,
                     OpenGlicForActiveTabAndDetach());
    RETURN_IF_ERROR(EnterLiveMode(instance));
    return instance;
  }

  // Opens glic for the active tab and waits until it is fully idle: the
  // invocation that opened the panel has terminated and the client has
  // finished loading.
  //
  // `OpenGlicForActiveTab()` only waits for the panel to be open; the
  // invocation that opened it may still be running. Tests that manipulate the
  // client load state must wait for it, otherwise that invocation observes the
  // manipulated state and records an extra `Glic.InvokeResult` sample. Driving
  // the open with an invocation of our own lets us wait for it to report
  // completion, which happens after `WaitForClientReadyTask` has run and so
  // also means the client is ready.
  [[nodiscard]] TestResult<GlicInstanceImpl*>
  OpenGlicForActiveTabAndWaitIdle() {
    tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
    base::test::TestFuture<void> success_future;
    GlicInvokeOptions options(glic::Target(*tab),
                              mojom::InvocationSource::kTopChromeButton);
    options.on_success = success_future.GetCallback();

    coordinator().Invoke(std::move(options));

    if (!success_future.Wait()) {
      return base::unexpected("Timed out opening glic for the active tab");
    }
    GlicInstanceImpl* instance = GetInstanceForTab(tab);
    if (!instance) {
      return base::unexpected("No glic instance is bound to the active tab");
    }
    return instance;
  }

  // Makes the web client go away for good by crashing the guest renderer, and
  // waits for the resulting load failure to reach the host.
  //
  // `Host` derives readiness from the web client connection, and hosts can
  // only report failure, so a test cannot fake a non-ready client while a
  // healthy one is connected; it has to actually become unusable. Navigating
  // the guest away does not work under `GlicNoWebview`, where the load is
  // dropped and the client stays up, so the renderer is killed instead: that
  // reaches the host the same way whichever way the client is hosted.
  [[nodiscard]] TestResult<> DisconnectWebClient(GlicInstanceImpl* instance) {
    content::RenderProcessHost* client_process =
        instance->host().GetWebClientRenderProcessHost();
    if (!client_process) {
      return base::unexpected("Glic has no web client render process");
    }
    {
      content::ScopedAllowRendererCrashes allow_crashes(client_process);
      content::RenderProcessHostWatcher process_gone(
          client_process,
          content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
      client_process->Shutdown(content::RESULT_CODE_KILLED);
      process_gone.Wait();
    }
    // Losing the client makes the host page report a load failure. Waiting for
    // it means the reported state has settled, so the tests below can drive it
    // without racing against that report.
    return RunUntilEqual([&]() { return instance->host().client_load_state(); },
                         ClientLoadState::kError,
                         "DisconnectWebClient: the client did not fail");
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeWithInvalidTab) {
  GlicHistogramTester histogram_tester;
  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.target.surface = tabs::TabHandle::Null();
  options.on_error = error_future.GetCallback();

  coordinator().Invoke(std::move(options));

  EXPECT_EQ(error_future.Get(), GlicInvokeError::kInvalidTab);
  histogram_tester.ExpectUniqueSample("Glic.Invoke.InvocationSource",
                                      mojom::InvocationSource::kOsButton, 1);
  histogram_tester.ExpectUniqueSample("Glic.InvokeResult",
                                      GlicInvokeError::kInvalidTab, 1);
  histogram_tester.ExpectUniqueSample("Glic.InvokeResult.OsButton",
                                      GlicInvokeError::kInvalidTab, 1);
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
  GlicHistogramTester histogram_tester;
  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.target.surface = handle;
  options.on_error = error_future.GetCallback();

  coordinator().Invoke(std::move(options));

  // 4. It should fail with GlicInvokeError::kTabClosed because the handle is
  // invalid now.
  EXPECT_EQ(error_future.Get(), GlicInvokeError::kTabClosed);
  histogram_tester.ExpectUniqueSample("Glic.Invoke.InvocationSource",
                                      mojom::InvocationSource::kOsButton, 1);
  histogram_tester.ExpectUniqueSample("Glic.InvokeResult",
                                      GlicInvokeError::kTabClosed, 1);
  histogram_tester.ExpectUniqueSample("Glic.InvokeResult.OsButton",
                                      GlicInvokeError::kTabClosed, 1);
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

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeWithValidConversationIdSmokeTest) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(
      glic::Target(*tab, glic::ConversationId("test-conversation-id")),
      mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();

  EXPECT_FALSE(GetInstanceForTab(tab));

  coordinator().Invoke(std::move(options));

  EXPECT_TRUE(success_future.Wait());
  EXPECT_TRUE(GetInstanceForTab(tab));
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeWithNewConversationSpawnsNewInstance) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(auto* instance1, OpenGlicForActiveTab());
  EXPECT_EQ(GetInstanceForTab(tab), instance1);

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab, glic::NewConversation{}),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();

  coordinator().Invoke(std::move(options));

  EXPECT_TRUE(success_future.Wait());

  auto* instance2 = GetInstanceForTab(tab);
  ASSERT_TRUE(instance2);
  EXPECT_NE(instance1, instance2);
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeFailsWhenProfileNotEnabled) {
  ScopedGlicCapability scoped_glic_capability(GetProfile(), false);

  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.on_error = error_future.GetCallback();
  options.target.surface = DefaultSurface{
      GetTabListInterface()->GetActiveTab()->GetBrowserWindowInterface()};

  coordinator().Invoke(std::move(options));

  EXPECT_EQ(error_future.Get(), GlicInvokeError::kProfileNotEnabled);
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       ToggleOpensSidePanelForAnchoredIneligibleUser) {
  GlicHistogramTester histogram_tester;
  ScopedGlicCapability scoped_glic_capability(GetProfile(), false);
  ASSERT_TRUE(GlicEnabling::HasConsentedForProfile(GetProfile()));
  ASSERT_TRUE(GlicEnabling::ShouldShowGlicButton(GetProfile()));
  ASSERT_FALSE(GlicEnabling::IsEnabledForProfile(GetProfile()));
  ASSERT_EQ(GlicEnabling::GetProfileReadyState(GetProfile()),
            mojom::ProfileReadyState::kIneligibleAccount);

  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab);

  service()->ToggleUI(tab->GetBrowserWindowInterface(),
                      /*prevent_close=*/false,
                      mojom::InvocationSource::kTopChromeButton);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator().IsPanelShowingForBrowser(
        *tab->GetBrowserWindowInterface());
  }));
  histogram_tester.ExpectBucketCount("Glic.InvokeResult",
                                     GlicInvokeError::kProfileNotEnabled, 0);

  // Toggling a second time should close the side panel cleanly.
  service()->ToggleUI(tab->GetBrowserWindowInterface(),
                      /*prevent_close=*/false,
                      mojom::InvocationSource::kTopChromeButton);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !coordinator().IsPanelShowingForBrowser(
        *tab->GetBrowserWindowInterface());
  }));
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeWithInvalidInstanceId) {
  base::test::TestFuture<GlicInvokeError> error_future;
  InstanceId invalid_id("non-existent-instance-id");
  GlicInvokeOptions options(glic::Target(invalid_id),
                            mojom::InvocationSource::kOsButton);
  options.on_error = error_future.GetCallback();
  options.target.surface = DefaultSurface{
      GetTabListInterface()->GetActiveTab()->GetBrowserWindowInterface()};

  coordinator().Invoke(std::move(options));

  EXPECT_EQ(error_future.Get(), GlicInvokeError::kInstanceNotFound);
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
  auto* instance_impl = static_cast<GlicInstanceImpl*>(instance.get());
  EXPECT_TRUE(instance_impl->host().IsWebClientConnected());

  // Verify that the passed instance is the correct one.
  EXPECT_EQ(instance.get(), GetInstanceForTab(tab));

  // The success callback should be called after full completion.
  EXPECT_TRUE(success_future.Wait());
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeWithoutActionableOptionsSucceeds) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();

  // Call invoke without any actionable fields (empty prompts, empty payload,
  // etc.).
  coordinator().Invoke(std::move(options));

  // The success callback should be called immediately after panel creation.
  EXPECT_TRUE(success_future.Wait());
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeWithActionableOptionsSucceeds) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.prompts.push_back("Actionable prompt");
  options.on_success = success_future.GetCallback();

  // Call invoke with an actionable field to ensure the IPC triggers correctly.
  coordinator().Invoke(std::move(options));

  // The success callback should be called after the web client acks the mojo
  // call.
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

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeWithOnPanelOpened) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());

  base::test::TestFuture<void> panel_opened_future;
  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.wait_for_panel_open = true;
  options.on_panel_opened = panel_opened_future.GetCallback();
  options.on_success = success_future.GetCallback();

  coordinator().Invoke(std::move(options));

  auto* instance = GetInstanceForTab(tab);
  ASSERT_TRUE(instance);

  // on_panel_opened should be called.
  EXPECT_TRUE(panel_opened_future.Wait());
  EXPECT_TRUE(instance->IsShowing());

  // success should also be called eventually.
  EXPECT_TRUE(success_future.Wait());
}

// Two invocations that both require a client invoke (here, because they carry
// prompts) cannot run simultaneously on the same instance.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       ClientInvokeWhileClientInvokeInProgress) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  GlicInvokeOptions options1(glic::Target(*tab),
                             mojom::InvocationSource::kOsButton);
  options1.prompts = {"first prompt"};

  coordinator().Invoke(std::move(options1));

  base::test::TestFuture<GlicInvokeError> error_future2;
  GlicInvokeOptions options2(glic::Target(*tab),
                             mojom::InvocationSource::kOsButton);
  options2.prompts = {"second prompt"};
  options2.on_error = error_future2.GetCallback();

  // Try to invoke again while the first one is still in progress for the same
  // instance.
  coordinator().Invoke(std::move(options2));

  // The second invoke should fail synchronously.
  EXPECT_EQ(error_future2.Get(), GlicInvokeError::kInvokeInProgress);
}

// Invocations that only show the UI don't send anything to the web client, so
// several of them may be in progress at once on the same instance.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       SimultaneousInvokesWithoutClientInvoke) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();

  base::test::TestFuture<GlicInvokeError> error_future1;
  base::test::TestFuture<void> success_future1;
  GlicInvokeOptions options1(glic::Target(*tab),
                             mojom::InvocationSource::kOsButton);
  options1.on_error = error_future1.GetCallback();
  options1.on_success = success_future1.GetCallback();

  coordinator().Invoke(std::move(options1));

  GlicInstanceImpl* instance = GetInstanceForTab(tab);
  ASSERT_TRUE(instance);

  base::test::TestFuture<GlicInvokeError> error_future2;
  base::test::TestFuture<void> success_future2;
  GlicInvokeOptions options2(glic::Target(*tab),
                             mojom::InvocationSource::kOsButton);
  options2.on_error = error_future2.GetCallback();
  options2.on_success = success_future2.GetCallback();

  // Invoke again, targeting the same instance, while the first invocation is
  // still in progress.
  coordinator().Invoke(std::move(options2));
  EXPECT_EQ(GetInstanceForTab(tab), instance);

  EXPECT_TRUE(success_future1.Wait());
  EXPECT_TRUE(success_future2.Wait());
  EXPECT_FALSE(error_future1.IsReady());
  EXPECT_FALSE(error_future2.IsReady());
}

// An invocation that requires a client invoke isn't blocked by an in-progress
// invocation that only shows the UI.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       ClientInvokeWhileShowOnlyInvokeInProgress) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();

  base::test::TestFuture<GlicInvokeError> error_future1;
  base::test::TestFuture<void> success_future1;
  GlicInvokeOptions options1(glic::Target(*tab),
                             mojom::InvocationSource::kOsButton);
  options1.on_error = error_future1.GetCallback();
  options1.on_success = success_future1.GetCallback();

  coordinator().Invoke(std::move(options1));

  GlicInstanceImpl* instance = GetInstanceForTab(tab);
  ASSERT_TRUE(instance);

  base::test::TestFuture<GlicInvokeError> error_future2;
  base::test::TestFuture<void> success_future2;
  GlicInvokeOptions options2(glic::Target(*tab),
                             mojom::InvocationSource::kOsButton);
  options2.prompts = {"a prompt"};
  options2.on_error = error_future2.GetCallback();
  options2.on_success = success_future2.GetCallback();

  coordinator().Invoke(std::move(options2));
  EXPECT_EQ(GetInstanceForTab(tab), instance);

  EXPECT_TRUE(success_future1.Wait());
  EXPECT_TRUE(success_future2.Wait());
  EXPECT_FALSE(error_future1.IsReady());
  EXPECT_FALSE(error_future2.IsReady());
}

// An invocation that only shows the UI isn't blocked by an in-progress
// invocation that requires a client invoke.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       ShowOnlyInvokeWhileClientInvokeInProgress) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();

  base::test::TestFuture<GlicInvokeError> error_future1;
  base::test::TestFuture<void> success_future1;
  GlicInvokeOptions options1(glic::Target(*tab),
                             mojom::InvocationSource::kOsButton);
  options1.prompts = {"a prompt"};
  options1.on_error = error_future1.GetCallback();
  options1.on_success = success_future1.GetCallback();

  coordinator().Invoke(std::move(options1));

  GlicInstanceImpl* instance = GetInstanceForTab(tab);
  ASSERT_TRUE(instance);

  base::test::TestFuture<GlicInvokeError> error_future2;
  base::test::TestFuture<void> success_future2;
  GlicInvokeOptions options2(glic::Target(*tab),
                             mojom::InvocationSource::kOsButton);
  options2.on_error = error_future2.GetCallback();
  options2.on_success = success_future2.GetCallback();

  coordinator().Invoke(std::move(options2));
  EXPECT_EQ(GetInstanceForTab(tab), instance);

  EXPECT_TRUE(success_future1.Wait());
  EXPECT_TRUE(success_future2.Wait());
  EXPECT_FALSE(error_future1.IsReady());
  EXPECT_FALSE(error_future2.IsReady());
}

// Once the first client invoke has completed, a second one can proceed.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       ClientInvokeAfterClientInvokeCompletes) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();

  base::test::TestFuture<void> success_future1;
  GlicInvokeOptions options1(glic::Target(*tab),
                             mojom::InvocationSource::kOsButton);
  options1.prompts = {"first prompt"};
  options1.on_success = success_future1.GetCallback();

  coordinator().Invoke(std::move(options1));
  EXPECT_TRUE(success_future1.Wait());

  base::test::TestFuture<GlicInvokeError> error_future2;
  base::test::TestFuture<void> success_future2;
  GlicInvokeOptions options2(glic::Target(*tab),
                             mojom::InvocationSource::kOsButton);
  options2.prompts = {"second prompt"};
  options2.on_error = error_future2.GetCallback();
  options2.on_success = success_future2.GetCallback();

  coordinator().Invoke(std::move(options2));

  EXPECT_TRUE(success_future2.Wait());
  EXPECT_FALSE(error_future2.IsReady());
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeSupersedesInProgress) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();

  base::test::TestFuture<GlicInvokeError> error_future1;
  GlicInvokeOptions options1(glic::Target(*tab),
                             mojom::InvocationSource::kOsButton);
  options1.prompts = {"first prompt"};
  options1.on_error = error_future1.GetCallback();

  coordinator().Invoke(std::move(options1));

  base::test::TestFuture<void> success_future2;
  GlicInvokeOptions options2(glic::Target(*tab),
                             mojom::InvocationSource::kOsButton);
  options2.prompts = {"second prompt"};
  options2.supersede_if_in_progress = true;
  options2.on_success = success_future2.GetCallback();

  // Try to invoke again while the first one is still in progress for the same
  // instance, but this time specify that it should supersede the first.
  coordinator().Invoke(std::move(options2));

  // The first invoke should fail synchronously.
  EXPECT_EQ(error_future1.Get(), GlicInvokeError::kSuperseded);

  // The second invoke should succeed.
  EXPECT_TRUE(success_future2.Wait());
}

// An invocation that only shows the UI never conflicts, so it has nothing to
// supersede: an in-progress client invoke is left alone.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       ShowOnlyInvokeDoesNotSupersedeClientInvoke) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();

  base::test::TestFuture<GlicInvokeError> error_future1;
  base::test::TestFuture<void> success_future1;
  GlicInvokeOptions options1(glic::Target(*tab),
                             mojom::InvocationSource::kOsButton);
  options1.prompts = {"a prompt"};
  options1.on_error = error_future1.GetCallback();
  options1.on_success = success_future1.GetCallback();

  coordinator().Invoke(std::move(options1));

  base::test::TestFuture<void> success_future2;
  GlicInvokeOptions options2(glic::Target(*tab),
                             mojom::InvocationSource::kOsButton);
  options2.supersede_if_in_progress = true;
  options2.on_success = success_future2.GetCallback();

  coordinator().Invoke(std::move(options2));

  EXPECT_TRUE(success_future1.Wait());
  EXPECT_TRUE(success_future2.Wait());
  EXPECT_FALSE(error_future1.IsReady());
}

// TODO(b/564473727): Test is flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_InvokeTimeoutBehaviors DISABLED_InvokeTimeoutBehaviors
#else
#define MAYBE_InvokeTimeoutBehaviors InvokeTimeoutBehaviors
#endif
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, MAYBE_InvokeTimeoutBehaviors) {
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

// TODO(crbug.com/564285975): Re-enable the test.
#if BUILDFLAG(IS_CHROMEOS) && !defined(NDEBUG)
#define MAYBE_InvokeFailsOnTabClosed DISABLED_InvokeFailsOnTabClosed
#else
#define MAYBE_InvokeFailsOnTabClosed InvokeFailsOnTabClosed
#endif
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, MAYBE_InvokeFailsOnTabClosed) {
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

  // The error should be kTabClosed.
  EXPECT_EQ(error_future.Get(), GlicInvokeError::kTabClosed);
}

// TODO(crbug.com/564531222): Fix flakiness and enable on linux-chromeos-dbg
// bot.
#if BUILDFLAG(IS_CHROMEOS) && !defined(NDEBUG)
#define MAYBE_InvokeFailsOnInstanceDestruction \
  DISABLED_InvokeFailsOnInstanceDestruction
#else
#define MAYBE_InvokeFailsOnInstanceDestruction InvokeFailsOnInstanceDestruction
#endif
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       MAYBE_InvokeFailsOnInstanceDestruction) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();

  ASSERT_OK(OpenGlicForActiveTab());

  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(glic::Target(*tab1),
                            mojom::InvocationSource::kOsButton);
  options.on_error = error_future.GetCallback();

  coordinator().Invoke(std::move(options));

  // Destroy the instance while Invoke is in progress.
  auto* instance = coordinator().GetInstanceForTab(tab1);
  ASSERT_TRUE(instance);
  coordinator().RemoveInstance(instance->id());

  // Since Glic was destroyed without the tab closing, the invocation should
  // fail with kInstanceDestroyed.
  EXPECT_EQ(error_future.Get(), GlicInvokeError::kInstanceDestroyed);
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
  GlicInvokeOptions options2(mojom::InvocationSource::kTabContextMenu);
  options2.target = Target(*tab2, instance->id());
  options2.tab_sharing =
      TabSharingOptions({tab2->GetHandle()}, GlicPinTrigger::kContextMenu);
  coordinator().Invoke(std::move(options2));

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

// Verifies that invoking with prompts doesn't cause any crashes or failures
// during the processing of options. Note: this acts as a smoke test and does
// not intercept the IPC to verify the prompts were actually delivered to the
// WebUI.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeWithPromptsSmokeTest) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();
  options.prompts = {"test prompt"};

  coordinator().Invoke(std::move(options));

  EXPECT_TRUE(success_future.Wait());

  GlicInstanceImpl* instance = GetInstanceForTab(tab);
  ASSERT_TRUE(instance);

  ASSERT_OK(WaitForGlicClient(instance));
}

// Verifies that invoking with a skill_id doesn't cause any crashes or failures
// during the processing of options. Note: this acts as a smoke test and does
// not intercept the IPC to verify the skill_id was actually delivered to the
// WebUI.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeWithSkillIdSmokeTest) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();
  options.skill_id = "test_skill_id";

  coordinator().Invoke(std::move(options));

  EXPECT_TRUE(success_future.Wait());

  GlicInstanceImpl* instance = GetInstanceForTab(tab);
  ASSERT_TRUE(instance);

  ASSERT_OK(WaitForGlicClient(instance));
}

// Verifies that invoking with an InvocationPayloadPtr doesn't cause crashes or
// failures during the mapping of source_or_payload.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeWithUniversalCartPayloadSmokeTest) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();

  base::test::TestFuture<void> success_future;
  // Construct options via payload branch instead of just InvocationSource enum.
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationPayload::NewUniversalCart(
                                mojom::UniversalCartPayload::New()));
  options.on_success = success_future.GetCallback();

  EXPECT_FALSE(GetInstanceForTab(tab));

  coordinator().Invoke(std::move(options));

  EXPECT_TRUE(success_future.Wait());

  GlicInstanceImpl* instance = GetInstanceForTab(tab);
  ASSERT_TRUE(instance);

  ASSERT_OK(WaitForGlicClient(instance));
}

// Verifies that invoking with an explicit TargetSurface actuation target
// successfully configures and pipes through.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeWithTargetSurfaceActuationTargetSmokeTest) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.target.actuation_target = mojom::ActuationTarget::kTargetSurface;
  options.on_success = success_future.GetCallback();

  EXPECT_FALSE(GetInstanceForTab(tab));

  coordinator().Invoke(std::move(options));

  EXPECT_TRUE(success_future.Wait());

  GlicInstanceImpl* instance = GetInstanceForTab(tab);
  ASSERT_TRUE(instance);

  ASSERT_OK(WaitForGlicClient(instance));
}

// TODO(crbug.com/528472503): Re-enable this test on Android once flakiness is
// fixed.
#if !BUILDFLAG(IS_ANDROID)
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

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeWithClipboardPolicyNavigationSuccess) {
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

  // Navigating the source tab after invocation starts should not cause the
  // paste policy check to fail.
  ASSERT_TRUE(content::NavigateToURL(tab->GetContents(),
                                     GURL("data:text/html,navigation")));

  EXPECT_TRUE(success_future.Wait());
  EXPECT_TRUE(GetInstanceForTab(tab));
}
#endif  // !BUILDFLAG(IS_ANDROID)

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
  GlicInvokeOptions options(glic::Target(*tab),
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

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeWithClipboardPolicyNoSourceFrame) {
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

  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_error = error_future.GetCallback();

  // Supply the additional context but omit the source frame ID (pass
  // default/null ID)
  options.additional_context = AdditionalTabContext(
      std::move(context_mojom), content::GlobalRenderFrameHostId(),
      PolicyCheck::kClipboard);

  coordinator().Invoke(std::move(options));

  EXPECT_EQ(error_future.Get(),
            GlicInvokeError::kAdditionalContextNoSourceFrame);
  EXPECT_FALSE(GetInstanceForTab(tab));
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeWithClipboardPolicyBlocked) {
  // Set up Data Controls to block clipboard copy/paste.
  {
    ScopedListPrefUpdate list(GetProfile()->GetPrefs(),
                              data_controls::kDataControlsRulesPref);
    list->Append(*base::JSONReader::Read(
        R"({
          "destinations": { "os_clipboard": true },
          "restrictions": [{
            "class": "CLIPBOARD",
            "level": "BLOCK"
          }]
        })",
        base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  }

  tabs::TabInterface* tab = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(content::NavigateToURL(tab->GetContents(), GURL("about:blank")));

  // Create mock AdditionalContext containing PNG image data.
  auto context_mojom = CreateMockAdditionalContext();

  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_error = error_future.GetCallback();

  content::RenderFrameHost* rfh = tab->GetContents()->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh);

  options.additional_context = AdditionalTabContext(
      std::move(context_mojom), rfh->GetGlobalId(), PolicyCheck::kClipboard);

  coordinator().Invoke(std::move(options));

  EXPECT_EQ(error_future.Get(),
            GlicInvokeError::kAdditionalContextFailedCopyPolicy);
  EXPECT_FALSE(GetInstanceForTab(tab));
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeWithClipboardPastePolicyBlockedFloating) {
  // Set up Data Controls to allow copying to the clipboard (`os_clipboard` not
  // restricted) but block pasting into any destination URL (`PastePolicyTask`).
  {
    ScopedListPrefUpdate list(GetProfile()->GetPrefs(),
                              data_controls::kDataControlsRulesPref);
    list->Append(*base::JSONReader::Read(
        R"({
          "destinations": { "urls": ["*"] },
          "restrictions": [{
            "class": "CLIPBOARD",
            "level": "BLOCK"
          }]
        })",
        base::JSON_PARSE_CHROMIUM_EXTENSIONS));
  }

  tabs::TabInterface* tab = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(content::NavigateToURL(tab->GetContents(), GURL("about:blank")));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenGlicForActiveTabAndDetach());
  ASSERT_TRUE(instance->IsDetached());

  auto context_mojom = CreateMockAdditionalContext();

  content::RenderFrameHost* rfh = tab->GetContents()->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh);

  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(mojom::InvocationSource::kWebDragDrop);
  options.additional_context = AdditionalTabContext(
      std::move(context_mojom), rfh->GetGlobalId(), PolicyCheck::kClipboard);
  // The tab handle here is only a fallback surface; GetInvokeTarget() targets
  // the floating surface because the floaty is the active embedder.
  options.target = instance->GetInvokeTarget(
      /*fallback_surface=*/glic::Target::Surface(tab->GetHandle()));
  options.on_error = error_future.GetCallback();

  coordinator().Invoke(std::move(options));

  EXPECT_EQ(error_future.Get(),
            GlicInvokeError::kAdditionalContextFailedPastePolicy);
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeWithContextNoSourceFrame) {
  tabs::TabInterface* tab = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(content::NavigateToURL(tab->GetContents(), GURL("about:blank")));

  // Create mock AdditionalContext containing PNG image data.
  auto context_mojom = CreateMockAdditionalContext();

  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_error = error_future.GetCallback();

  // Provide an invalid GlobalRenderFrameHostId to simulate the frame being
  // destroyed before the invoke task starts.
  options.additional_context = AdditionalTabContext(
      std::move(context_mojom), content::GlobalRenderFrameHostId(-1, -1),
      PolicyCheck::kClipboard);

  coordinator().Invoke(std::move(options));

  // The policy check should fail because it cannot find the source frame.
  EXPECT_EQ(error_future.Get(),
            GlicInvokeError::kAdditionalContextNoSourceFrame);
  EXPECT_FALSE(GetInstanceForTab(tab));
}
class GlicInvokeBrowserTestWithoutActor : public GlicInvokeBrowserTest {
 public:
  GlicInvokeBrowserTestWithoutActor() {
    scoped_feature_list_.InitAndDisableFeature(::features::kGlicActor);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTestWithoutActor,
                       InvokeWithInvalidConfiguration) {
  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.target.surface = DefaultSurface{
      GetTabListInterface()->GetActiveTab()->GetBrowserWindowInterface()};
  // Invoking with an actuating feature mode when ActorKeyedService is not
  // enabled will result in kInvalidConfiguration.
  options.feature_mode = mojom::FeatureMode::kActuation;
  options.on_error = error_future.GetCallback();

  coordinator().Invoke(std::move(options));

  EXPECT_EQ(error_future.Get(), GlicInvokeError::kInvalidConfiguration);
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeWithInvalidContextData) {
  tabs::TabInterface* tab = CreateUserInitiatedTab(GURL("about:blank"));
  ASSERT_TRUE(content::NavigateToURL(tab->GetContents(), GURL("about:blank")));

  // Create mock AdditionalContext with an invalid mime_type.
  // ExtractThumbnailData() in glic_invoke_task.cc strictly checks for
  // "image/png", so "image/jpeg" will cause thumbnail_data to be empty.
  auto context_mojom = CreateMockAdditionalContext(
      "image/jpeg", std::vector<uint8_t>{0xFF, 0xD8, 0xFF, 0xE0});

  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_error = error_future.GetCallback();

  content::RenderFrameHost* rfh = tab->GetContents()->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh);

  // Trigger clipboard policy check.
  options.additional_context = AdditionalTabContext(
      std::move(context_mojom), rfh->GetGlobalId(), PolicyCheck::kClipboard);

  coordinator().Invoke(std::move(options));

  EXPECT_EQ(error_future.Get(),
            GlicInvokeError::kAdditionalContextNoClipboardMetadata);

  // Since we failed the copy policy check (which happens first), we will have
  // stopped the flow before creating a glic instance.
  EXPECT_FALSE(GetInstanceForTab(tab));
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeHiddenClientRevertsVisibilityOnFailure) {
  tabs::TabInterface* tab = CreateUserInitiatedTab(GURL("about:blank"));
  ASSERT_TRUE(content::NavigateToURL(tab->GetContents(), GURL("about:blank")));

  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_error = error_future.GetCallback();
  // Cancel the invocation after the client has connected.
  options.on_client_connected =
      base::BindOnce([](base::WeakPtr<GlicInstance> instance) {
        if (instance) {
          instance->CancelInvoke();
        }
      });

  GlicInvokeWithAutoSubmitOptions auto_submit_options;
  auto_submit_options.show_panel = false;

  auto instance_wp = coordinator().InvokeWithAutoSubmit(
      GetPassKey(), std::move(options), std::move(auto_submit_options));

  EXPECT_EQ(error_future.Get(), GlicInvokeError::kCancelled);

  ASSERT_TRUE(instance_wp);
  auto* host = &static_cast<GlicInstanceImpl*>(instance_wp.get())->host();

  content::WebContents* contents =
      host->contents_manager()->active_web_contents();
  if (contents) {
    EXPECT_EQ(contents->GetVisibility(), content::Visibility::HIDDEN);
  }
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeWithInvalidContextMultipleFormats) {
  tabs::TabInterface* tab = CreateUserInitiatedTab(GURL("about:blank"));
  ASSERT_TRUE(content::NavigateToURL(tab->GetContents(), GURL("about:blank")));

  // Create mock AdditionalContext with both image/png and text formats.
  auto context_mojom = CreateMockAdditionalContext();

  auto text_data = mojom::ContextData::New();
  text_data->mime_type = kMimeTypeGlicSelection;
  std::string my_text = "test";
  text_data->data = mojo_base::BigBuffer(
      std::vector<uint8_t>(my_text.begin(), my_text.end()));
  context_mojom->parts.push_back(
      mojom::AdditionalContextPart::NewData(std::move(text_data)));

  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_error = error_future.GetCallback();

  content::RenderFrameHost* rfh = tab->GetContents()->GetPrimaryMainFrame();
  ASSERT_TRUE(rfh);

  // Trigger clipboard policy check. This will fail with kInvalidConfiguration
  // because having both formats is invalid for the clipboard metadata.
  options.additional_context = AdditionalTabContext(
      std::move(context_mojom), rfh->GetGlobalId(), PolicyCheck::kClipboard);

  coordinator().Invoke(std::move(options));

  EXPECT_EQ(error_future.Get(), GlicInvokeError::kInvalidConfiguration);
  EXPECT_FALSE(GetInstanceForTab(tab));
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
  auto usage = instance->GetSharingManagerInternal().GetPinnedTabUsage(
      tab2->GetHandle());
  ASSERT_TRUE(usage.has_value());
  EXPECT_EQ(usage->pin_event.trigger, GlicPinTrigger::kInstanceCreation);
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeWithPinOnBindFalse) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();
  options.pin_on_bind = false;

  coordinator().Invoke(std::move(options));

  EXPECT_TRUE(success_future.Wait());

  auto* instance = GetInstanceForTab(tab);
  ASSERT_TRUE(instance);

  EXPECT_FALSE(
      instance->GetSharingManagerInternal().IsTabPinned(tab->GetHandle()));
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

    EXPECT_TRUE(
        std::holds_alternative<GlicInvokeHandler::TabSurface>(resolved));
    auto tab_surface = std::get<GlicInvokeHandler::TabSurface>(resolved);
    EXPECT_TRUE(tab_surface.is_new);
    ASSERT_TRUE(tab_surface.tab);

    // Verify it created an incognito browser.
    new_browser = ProfileBrowserCollection::GetForProfile(incognito_profile)
                      ->GetLastActiveBrowser();
    ASSERT_TRUE(new_browser);
    EXPECT_TRUE(new_browser->GetProfile()->IsOffTheRecord());
  }

  // Clean up the new window.
  CloseBrowserSynchronously(new_browser);
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       ResolveTargetSurfaceSkipsAppWindow) {
  BrowserWindowInterface* app_browser =
      CreateBrowserWindow(BrowserWindowCreateParams::CreateForApp(
          "test_app", /*trusted_source=*/true, gfx::Rect(), GetProfile(),
          /*user_gesture=*/true));
  app_browser->GetWindow()->Show();

  // 1. DefaultSurface targeting app_browser falls back to a normal browser.
  {
    BrowserWindowInterface* fallback_browser = nullptr;
    {
      Target target;
      target.surface = DefaultSurface{app_browser};
      auto resolved =
          GlicInvokeHandler::ResolveTargetSurface(GetProfile(), target);
      ASSERT_TRUE(
          std::holds_alternative<GlicInvokeHandler::TabSurface>(resolved));
      auto tab_surface = std::get<GlicInvokeHandler::TabSurface>(resolved);
      fallback_browser = tab_surface.tab->GetBrowserWindowInterface();
      EXPECT_NE(fallback_browser, app_browser);
    }
    CloseBrowserSynchronously(fallback_browser);
  }

  // 2. NewTab targeting app_browser falls back to a normal browser.
  {
    BrowserWindowInterface* fallback_browser = nullptr;
    {
      Target target;
      target.surface = NewTab{app_browser};
      auto resolved =
          GlicInvokeHandler::ResolveTargetSurface(GetProfile(), target);
      ASSERT_TRUE(
          std::holds_alternative<GlicInvokeHandler::TabSurface>(resolved));
      auto tab_surface = std::get<GlicInvokeHandler::TabSurface>(resolved);
      fallback_browser = tab_surface.tab->GetBrowserWindowInterface();
      EXPECT_NE(fallback_browser, app_browser);
    }
    CloseBrowserSynchronously(fallback_browser);
  }

  // 3. TabHandle targeting app_browser's tab is rejected (resolves to nullptr).
  {
    tabs::TabInterface* app_tab =
        TabListInterface::From(app_browser)
            ->OpenTab(GURL("about:blank"), -1, /*foreground=*/true);
    ASSERT_TRUE(app_tab);
    Target target;
    target.surface = app_tab->GetHandle();
    auto resolved =
        GlicInvokeHandler::ResolveTargetSurface(GetProfile(), target);
    ASSERT_TRUE(
        std::holds_alternative<GlicInvokeHandler::TabSurface>(resolved));
    auto tab_surface = std::get<GlicInvokeHandler::TabSurface>(resolved);
    EXPECT_EQ(tab_surface.tab, nullptr);

    base::test::TestFuture<GlicInvokeError> error_future;
    GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
    options.target = std::move(target);
    options.on_error = error_future.GetCallback();
    coordinator().Invoke(std::move(options));
    EXPECT_EQ(error_future.Get(), GlicInvokeError::kInvalidTab);
  }

  CloseBrowserSynchronously(app_browser);
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, ResolveTargetSurfaceDetachedTab) {
  // Open a new background tab and detach it to simulate a detached background
  // tab.
  tabs::TabInterface* tab = GetTabListInterface()->OpenTab(
      GURL("about:blank"), -1, /*foreground=*/false);
  ASSERT_TRUE(tab);
  BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
  ASSERT_TRUE(browser);
  int tab_index = browser->GetTabStripModel()->GetIndexOfTab(tab);
  ASSERT_GE(tab_index, 0);

  std::unique_ptr<tabs::TabModel> detached_tab =
      browser->GetTabStripModel()->DetachTabAtForInsertion(tab_index);
  ASSERT_TRUE(detached_tab);
  EXPECT_EQ(detached_tab->GetBrowserWindowInterface(), nullptr);

  // 1. Without background actuation target, detached tab is rejected.
  {
    Target target;
    target.surface = detached_tab->GetHandle();
    target.actuation_target = mojom::ActuationTarget::kAgentDecides;
    auto resolved =
        GlicInvokeHandler::ResolveTargetSurface(GetProfile(), target);
    ASSERT_TRUE(
        std::holds_alternative<GlicInvokeHandler::TabSurface>(resolved));
    auto tab_surface = std::get<GlicInvokeHandler::TabSurface>(resolved);
    EXPECT_EQ(tab_surface.tab, nullptr);
  }

  // 2. With background actuation target (kTargetSurface), detached tab is
  // allowed.
  {
    Target target;
    target.surface = detached_tab->GetHandle();
    target.actuation_target = mojom::ActuationTarget::kTargetSurface;
    auto resolved =
        GlicInvokeHandler::ResolveTargetSurface(GetProfile(), target);
    ASSERT_TRUE(
        std::holds_alternative<GlicInvokeHandler::TabSurface>(resolved));
    auto tab_surface = std::get<GlicInvokeHandler::TabSurface>(resolved);
    EXPECT_EQ(tab_surface.tab, detached_tab.get());
  }

  // Reattach the detached tab before test ends.
  browser->GetTabStripModel()->InsertDetachedTabAt(
      tab_index, std::move(detached_tab), AddTabTypes::ADD_NONE);
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, ResolveTargetSurfaceFloating) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenGlicForActiveTabAndDetach());
  ASSERT_TRUE(instance->IsDetached());

  Target target = instance->GetInvokeTarget(Target::Surface());
  EXPECT_TRUE(std::holds_alternative<Floating>(target.surface));

  GlicInvokeHandler::ResolvedTarget resolved =
      GlicInvokeHandler::ResolveTargetSurface(GetProfile(), target);

  EXPECT_TRUE(std::holds_alternative<Floating>(resolved));
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeWithFloatingTarget) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenGlicForActiveTabAndDetach());
  ASSERT_TRUE(instance->IsDetached());

  Target target = instance->GetInvokeTarget(Target::Surface());

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(std::move(target),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();

  coordinator().Invoke(std::move(options));

  EXPECT_TRUE(success_future.Wait());
  EXPECT_TRUE(instance->IsDetached());
}

// The floaty is kept when the targeted tab is already bound to it.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       PreserveActiveSurfaceKeepsBoundTabInFloaty) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenGlicForActiveTabAndDetach());
  ASSERT_TRUE(instance->IsActiveEmbedder(FloatingEmbedderKey{}));

  // Detaching leaves the tab bound to the instance.
  ASSERT_EQ(GetInstanceForTab(tab), instance);

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.preserve_active_surface = true;
  options.on_success = success_future.GetCallback();
  coordinator().Invoke(std::move(options));
  ASSERT_TRUE(success_future.Wait());

  EXPECT_TRUE(instance->IsActiveEmbedder(FloatingEmbedderKey{}));
  EXPECT_FALSE(instance->IsActiveEmbedder(SidePanelEmbedderKey(tab)));
}

// The option is opt-in; the same invocation without it moves to the tab.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       WithoutPreserveActiveSurfaceFloatyMovesToSidePanel) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenGlicForActiveTabAndDetach());
  ASSERT_TRUE(instance->IsActiveEmbedder(FloatingEmbedderKey{}));
  ASSERT_EQ(GetInstanceForTab(tab), instance);

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();
  coordinator().Invoke(std::move(options));
  ASSERT_TRUE(success_future.Wait());

  ASSERT_OK(WaitForActiveEmbedderToMatchTab(instance, tab));
  EXPECT_FALSE(instance->IsActiveEmbedder(FloatingEmbedderKey{}));
}

// Targeting the floaty's conversation explicitly binds the tab to it.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       PreserveActiveSurfaceBindsTargetedTabToFloaty) {
  // Stop the instance from binding new tabs on its own.
  GetProfile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled, false);

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenGlicForActiveTabAndDetach());
  ASSERT_TRUE(instance->IsActiveEmbedder(FloatingEmbedderKey{}));

  tabs::TabInterface* tab = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(tab);
  ASSERT_FALSE(GetInstanceForTab(tab));

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(
      glic::Target(*tab, glic::InstanceId(instance->id())),
      mojom::InvocationSource::kOsButton);
  options.preserve_active_surface = true;
  options.on_success = success_future.GetCallback();
  coordinator().Invoke(std::move(options));
  ASSERT_TRUE(success_future.Wait());

  EXPECT_TRUE(instance->IsActiveEmbedder(FloatingEmbedderKey{}));
  EXPECT_FALSE(instance->IsActiveEmbedder(SidePanelEmbedderKey(tab)));
  EXPECT_EQ(GetInstanceForTab(tab), instance);
  EXPECT_EQ(coordinator().GetInstances().size(), 1u);
}

// `DefaultConversation` asks for the tab's own conversation, so an unbound tab
// is not adopted by the floaty.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       PreserveActiveSurfaceLeavesUnboundTabOnItsOwnInstance) {
  // Stop the instance from binding new tabs on its own.
  GetProfile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled, false);

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenGlicForActiveTabAndDetach());
  ASSERT_TRUE(instance->IsActiveEmbedder(FloatingEmbedderKey{}));

  tabs::TabInterface* tab = CreateAndActivateTab(GURL("about:blank"));
  ASSERT_TRUE(tab);
  ASSERT_FALSE(GetInstanceForTab(tab));

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.preserve_active_surface = true;
  options.on_success = success_future.GetCallback();
  coordinator().Invoke(std::move(options));
  ASSERT_TRUE(success_future.Wait());

  EXPECT_TRUE(instance->IsActiveEmbedder(FloatingEmbedderKey{}));
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * tab_instance,
                       WaitForGlicInstanceBoundToTab(tab));
  EXPECT_NE(tab_instance, instance);
  ASSERT_OK(WaitForActiveEmbedderToMatchTab(tab_instance, tab));
  EXPECT_EQ(coordinator().GetInstances().size(), 2u);
}

// `kProceedInLiveMode` is the default: the conversation carries on in live
// mode in the floaty, rather than being pulled into the targeted tab's side
// panel. `preserve_active_surface` is not needed (or consulted) for this.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       LiveModeProceedInLiveModeKeepsConversationInFloaty) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenLiveModeFloatyForActiveTab());
  ASSERT_EQ(GetInstanceForTab(tab), instance);

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  ASSERT_EQ(options.target.live_mode_behavior,
            LiveModeBehavior::kProceedInLiveMode);
  ASSERT_FALSE(options.preserve_active_surface);
  options.on_success = success_future.GetCallback();
  coordinator().Invoke(std::move(options));
  ASSERT_TRUE(success_future.Wait());

  EXPECT_TRUE(instance->IsActiveEmbedder(FloatingEmbedderKey{}));
  EXPECT_FALSE(instance->IsActiveEmbedder(SidePanelEmbedderKey(tab)));
}

// `kProceedInLiveMode` leaves the conversation where it is, so a live mode
// conversation in a side panel isn't dragged into the floaty.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       LiveModeProceedInLiveModeKeepsSidePanelConversation) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForActiveEmbedderToMatchTab(instance, tab));
  ASSERT_OK(EnterLiveMode(instance));

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();
  coordinator().Invoke(std::move(options));
  ASSERT_TRUE(success_future.Wait());

  EXPECT_TRUE(instance->IsActiveEmbedder(SidePanelEmbedderKey(tab)));
  EXPECT_FALSE(instance->IsActiveEmbedder(FloatingEmbedderKey{}));
}

// In live mode the behavior supersedes `preserve_active_surface`, which would
// otherwise keep the conversation in the floaty.
IN_PROC_BROWSER_TEST_F(
    GlicInvokeBrowserTest,
    LiveModeForceSidePanelTextModeSupersedesPreserveActiveSurface) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenLiveModeFloatyForActiveTab());
  ASSERT_EQ(GetInstanceForTab(tab), instance);

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.preserve_active_surface = true;
  options.target.live_mode_behavior = LiveModeBehavior::kForceSidePanelTextMode;
  options.on_success = success_future.GetCallback();
  coordinator().Invoke(std::move(options));
  ASSERT_TRUE(success_future.Wait());

  ASSERT_OK(WaitForActiveEmbedderToMatchTab(instance, tab));
  EXPECT_FALSE(instance->IsActiveEmbedder(FloatingEmbedderKey{}));
}

// `preserve_active_surface` still applies when the instance isn't in live
// mode, even if `kForceSidePanelTextMode` was requested.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       LiveModeForceSidePanelTextModeIgnoredWhenNotInLiveMode) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenGlicForActiveTabAndDetach());
  ASSERT_FALSE(instance->IsLiveMode());
  ASSERT_EQ(GetInstanceForTab(tab), instance);

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.preserve_active_surface = true;
  options.target.live_mode_behavior = LiveModeBehavior::kForceSidePanelTextMode;
  options.on_success = success_future.GetCallback();
  coordinator().Invoke(std::move(options));
  ASSERT_TRUE(success_future.Wait());

  EXPECT_TRUE(instance->IsActiveEmbedder(FloatingEmbedderKey{}));
  EXPECT_FALSE(instance->IsActiveEmbedder(SidePanelEmbedderKey(tab)));
}

// `kForceFloatingTextMode` keeps the conversation in the floaty, without the
// caller having to also set `preserve_active_surface`.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       LiveModeForceFloatingTextModeKeepsConversationInFloaty) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenLiveModeFloatyForActiveTab());
  ASSERT_EQ(GetInstanceForTab(tab), instance);

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.target.live_mode_behavior = LiveModeBehavior::kForceFloatingTextMode;
  options.on_success = success_future.GetCallback();
  coordinator().Invoke(std::move(options));
  ASSERT_TRUE(success_future.Wait());

  EXPECT_TRUE(instance->IsActiveEmbedder(FloatingEmbedderKey{}));
  EXPECT_FALSE(instance->IsActiveEmbedder(SidePanelEmbedderKey(tab)));
}

// A live mode conversation showing in a side panel is moved into the floaty,
// which is asked to open in text mode.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       LiveModeForceFloatingTextModeRequestsTextMode) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForActiveEmbedderToMatchTab(instance, tab));
  ASSERT_OK(EnterLiveMode(instance));

  GlicHistogramTester histogram_tester;
  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.target.live_mode_behavior = LiveModeBehavior::kForceFloatingTextMode;
  options.on_success = success_future.GetCallback();
  coordinator().Invoke(std::move(options));
  ASSERT_TRUE(success_future.Wait());

  EXPECT_TRUE(instance->IsActiveEmbedder(FloatingEmbedderKey{}));
  histogram_tester.ExpectUniqueSample("Glic.Instance.Floaty.InitialMode",
                                      mojom::WebClientMode::kText, 1);
}

// `kFail` rejects the invocation rather than disrupting live mode.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, LiveModeFailRejectsInvocation) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenLiveModeFloatyForActiveTab());
  ASSERT_EQ(GetInstanceForTab(tab), instance);

  GlicHistogramTester histogram_tester;
  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.target.live_mode_behavior = LiveModeBehavior::kFail;
  options.on_error = error_future.GetCallback();
  coordinator().Invoke(std::move(options));

  EXPECT_EQ(error_future.Get(), GlicInvokeError::kLiveModeActive);
  histogram_tester.ExpectUniqueSample("Glic.InvokeResult",
                                      GlicInvokeError::kLiveModeActive, 1);
  // The conversation is left where it was.
  EXPECT_TRUE(instance->IsActiveEmbedder(FloatingEmbedderKey{}));
}

// `kFail` only applies to instances that are in live mode.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       LiveModeFailIgnoredWhenNotInLiveMode) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_FALSE(instance->IsLiveMode());

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.target.live_mode_behavior = LiveModeBehavior::kFail;
  options.on_success = success_future.GetCallback();
  coordinator().Invoke(std::move(options));

  EXPECT_TRUE(success_future.Wait());
}

// `kForceSidePanelTextMode` steers the invocation towards a tab's side panel,
// so it can't be combined with a surface that always resolves to the floaty.
IN_PROC_BROWSER_TEST_F(
    GlicInvokeBrowserTest,
    LiveModeForceSidePanelTextModeWithFloatingTargetIsInvalid) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenLiveModeFloatyForActiveTab());

  GlicHistogramTester histogram_tester;
  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(instance->GetInvokeTarget(Target::Surface()),
                            mojom::InvocationSource::kOsButton);
  ASSERT_TRUE(std::holds_alternative<Floating>(options.target.surface));
  options.target.live_mode_behavior = LiveModeBehavior::kForceSidePanelTextMode;
  options.on_error = error_future.GetCallback();
  coordinator().Invoke(std::move(options));

  EXPECT_EQ(error_future.Get(), GlicInvokeError::kInvalidConfiguration);
  histogram_tester.ExpectUniqueSample(
      "Glic.InvokeResult", GlicInvokeError::kInvalidConfiguration, 1);
}

// `kForceFloatingTextMode` can steer a `Floating` target: the conversation is
// already where it wants it, and the text mode request still applies.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       LiveModeForceFloatingTextModeWithFloatingTargetIsValid) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenLiveModeFloatyForActiveTab());

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(instance->GetInvokeTarget(Target::Surface()),
                            mojom::InvocationSource::kOsButton);
  options.target.live_mode_behavior = LiveModeBehavior::kForceFloatingTextMode;
  options.on_success = success_future.GetCallback();
  coordinator().Invoke(std::move(options));

  ASSERT_TRUE(success_future.Wait());
  EXPECT_TRUE(instance->IsActiveEmbedder(FloatingEmbedderKey{}));
}

// `LastActiveOrNew` follows whichever surface the instance is already on, so
// neither of the surface-choosing behaviors could take effect.
IN_PROC_BROWSER_TEST_F(
    GlicInvokeBrowserTest,
    LiveModeForceSidePanelTextModeWithLastActiveOrNewIsInvalid) {
  BrowserWindowInterface* window =
      GetTabListInterface()->GetActiveTab()->GetBrowserWindowInterface();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenLiveModeFloatyForActiveTab());

  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.target.conversation = glic::InstanceId(instance->id());
  options.target.surface =
      glic::LastActiveOrNew{window, /*open_in_foreground=*/true};
  options.target.live_mode_behavior = LiveModeBehavior::kForceSidePanelTextMode;
  options.on_error = error_future.GetCallback();
  coordinator().Invoke(std::move(options));

  EXPECT_EQ(error_future.Get(), GlicInvokeError::kInvalidConfiguration);
}

IN_PROC_BROWSER_TEST_F(
    GlicInvokeBrowserTest,
    LiveModeForceFloatingTextModeWithLastActiveOrNewIsInvalid) {
  BrowserWindowInterface* window =
      GetTabListInterface()->GetActiveTab()->GetBrowserWindowInterface();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenLiveModeFloatyForActiveTab());

  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.target.conversation = glic::InstanceId(instance->id());
  options.target.surface =
      glic::LastActiveOrNew{window, /*open_in_foreground=*/true};
  options.target.live_mode_behavior = LiveModeBehavior::kForceFloatingTextMode;
  options.on_error = error_future.GetCallback();
  coordinator().Invoke(std::move(options));

  EXPECT_EQ(error_future.Get(), GlicInvokeError::kInvalidConfiguration);
}

// `kFail` doesn't choose a surface, so it works with any of them.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       LiveModeFailWithLastActiveOrNewRejectsInvocation) {
  BrowserWindowInterface* window =
      GetTabListInterface()->GetActiveTab()->GetBrowserWindowInterface();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance,
                       OpenLiveModeFloatyForActiveTab());

  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.target.conversation = glic::InstanceId(instance->id());
  options.target.surface =
      glic::LastActiveOrNew{window, /*open_in_foreground=*/true};
  options.target.live_mode_behavior = LiveModeBehavior::kFail;
  options.on_error = error_future.GetCallback();
  coordinator().Invoke(std::move(options));

  EXPECT_EQ(error_future.Get(), GlicInvokeError::kLiveModeActive);
}

// An instance can be bound to several tabs at once, but only one of its side
// panels is active at a time. An invocation that targets a specific tab must
// show on that tab, rather than following the instance to wherever it happens
// to be active. The two tabs live in separate windows so that both remain
// activated (an invocation on a non-activated tab would only create an
// inactive side panel). Desktop only: Android has a single active embedder and
// peeks the background window's tab instead of activating it.
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeTargetsRequestedTabWhenActiveInAnotherTab) {
  // Keep the instance from binding to newly created tabs on its own, so that
  // the bindings under test are only the ones this test sets up.
  GetProfile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicKeepSidepanelOpenOnNewTabsEnabled, false);

  tabs::TabInterface* tab_a = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForActiveEmbedderToMatchTab(instance, tab_a));

  // Bind the instance to tab B, in a second window, and leave it active there.
  // This goes through Show() rather than Invoke() so that the setup does not
  // depend on the invocation routing that this test is exercising.
  BrowserWindowInterface* browser_b = CreateAdditionalBrowserWindow();
  tabs::TabInterface* tab_b =
      CreateAndActivateTab(browser_b, GURL("about:blank"));
  ASSERT_TRUE(tab_b);
  instance->Show(ShowOptions::ForSidePanel(*tab_b));
  ASSERT_OK(WaitForGlicInstanceBoundToTab(tab_b));
  ASSERT_OK(WaitForActiveEmbedderToMatchTab(instance, tab_b));

  // The instance is now bound to both tabs, and active on tab B.
  ASSERT_EQ(GetInstanceForTab(tab_a), instance);
  ASSERT_EQ(GetInstanceForTab(tab_b), instance);
  ASSERT_TRUE(tab_a->IsActivated());

  // Invoke against tab A while the instance is still active on tab B.
  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab_a),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();
  coordinator().Invoke(std::move(options));
  ASSERT_TRUE(success_future.Wait());

  // The invocation must move to the targeted tab, not stay on tab B.
  ASSERT_OK(WaitForActiveEmbedderToMatchTab(instance, tab_a));

  // The invocation should have reused the existing instance.
  EXPECT_EQ(GetInstanceForTab(tab_a), instance);
  EXPECT_EQ(coordinator().GetInstances().size(), 1u);
}

#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/504753617): Re-enable the test.
// TODO(b/565538504): Re-enable on Android.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
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
                       InvokeWaitsForFreCompletion_AlwaysWaitMode) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  SetFRECompletion(GetProfile(), prefs::FreStatus::kNotStarted);

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(Target(*tab), mojom::InvocationSource::kOsButton);
  options.fre_override = mojom::FreOverride::kTrustFirstClick;
  options.fre_completion_wait_mode = FreCompletionWaitMode::kAlways;
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
                       InvokeDoesNotWaitForFreCompletion_DefaultWaitMode) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  SetFRECompletion(GetProfile(), prefs::FreStatus::kNotStarted);

  base::test::TestFuture<void> success_future;
  // With kDefault and an invocation source that doesn't mandate a client invoke
  // (like kOsButton), we skip waiting for FRE completion.
  GlicInvokeOptions options(Target(*tab), mojom::InvocationSource::kOsButton);
  options.fre_override = mojom::FreOverride::kTrustFirstClick;
  options.fre_completion_wait_mode = FreCompletionWaitMode::kDefault;
  options.on_success = success_future.GetCallback();

  coordinator().Invoke(std::move(options));

  // The success callback SHOULD be called immediately because FRE wait is
  // skipped.
  EXPECT_TRUE(success_future.Wait());
}

IN_PROC_BROWSER_TEST_F(
    GlicInvokeBrowserTest,
    InvokeWaitsForFreCompletion_DefaultWaitModeWithClientInvoke) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  SetFRECompletion(GetProfile(), prefs::FreStatus::kNotStarted);

  base::test::TestFuture<void> success_future;
  // With kDefault and an invocation source that mandates a client invoke
  // (like kCaptureRegionHotkey), we should block on FRE completion.
  GlicInvokeOptions options(Target(*tab),
                            mojom::InvocationSource::kCaptureRegionHotkey);
  options.fre_override = mojom::FreOverride::kTrustFirstClick;
  options.fre_completion_wait_mode = FreCompletionWaitMode::kDefault;
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

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeDoesNotWaitForFreCompletion_ModeNever) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  SetFRECompletion(GetProfile(), prefs::FreStatus::kNotStarted);

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  // Setting kTrustFirstClick which normally forces it to block...
  options.fre_override = mojom::FreOverride::kTrustFirstClick;
  // ... but kNever should override this and ensure it proceeds immediately.
  options.fre_completion_wait_mode = FreCompletionWaitMode::kNever;
  options.on_success = success_future.GetCallback();
  options.target = Target(*tab);

  coordinator().Invoke(std::move(options));

  // The invocation should complete successfully right away without waiting for
  // FRE.
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

  // Flush the message loop to ensure no incorrect async completion tasks run.
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

// TODO(b/477918640): Tests habe been failing consistently on ChromeOS, Linux and Android.
IN_PROC_BROWSER_TEST_F(GlicInvokeActuationBrowserTest,
                       DISABLED_InvokeDoesNotFailOnTabClosedAfterActuationStarts) {
  // Add a new tab so we don't close the browser when we close the active tab.
  tabs::TabInterface* tab2 = CreateAndActivateTab(GURL("about:blank"));

  // Go back to the original tab and open Glic.
  tabs::TabInterface* tab = GetTabListInterface()->GetTab(0);
  ActivateTab(tab);

  base::test::TestFuture<void> success_future;
  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);
  options.feature_mode = mojom::FeatureMode::kActuation;
  options.on_success = success_future.GetCallback();
  options.on_error = error_future.GetCallback();
  options.target = Target(*tab);

  // 1. Trigger invocation in actuation mode.
  coordinator().Invoke(std::move(options));

  auto* instance = GetInstanceForTab(tab);
  ASSERT_TRUE(instance);
  auto weak_instance = static_cast<GlicInstanceImpl*>(instance)->GetWeakPtr();

  // Pin tab2 to the same instance to keep it alive when tab is closed.
  weak_instance->GetSharingManagerInternal().PinTabs({tab2->GetHandle()},
                                                     GlicPinTrigger::kUnknown);

  // Wait until the web client is connected.
  ASSERT_OK(WaitForGlicClient(weak_instance.get()));

  // 2. Simulate actuation starting by creating an actor task.
  auto task_id_result = CreateActorTask(weak_instance.get());
  ASSERT_TRUE(task_id_result.has_value()) << task_id_result.error();
  auto task_id = task_id_result.value();

  EXPECT_TRUE(weak_instance->IsActuating());

  // Spin the message loop to ensure that GlicInvokeHandler processes the
  // actuation state change.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // Ensure the renderer has processed the invoke mojo IPC before we freeze it
  // by closing the tab.
  EXPECT_EQ(true,
            content::EvalJs(weak_instance->host().webui_contents(), "true"));

  // 3. Close the tab. This should NOT fail the invocation because actuation
  // started.
  tab->Close();

  // Flush the message loop to ensure that no asynchronous error tasks were
  // posted as a result of closing the tab.
  base::RunLoop run_loop_close;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop_close.QuitClosure());
  run_loop_close.Run();

  // The invocation should STILL not be complete (nor failed) because actuation
  // is ongoing.
  EXPECT_FALSE(success_future.IsReady());
  EXPECT_FALSE(error_future.IsReady());

  if (!weak_instance) {
    return;
  }

  // 4. Simulate actuation finishing.
  if (auto* session =
          weak_instance->GetActorTaskManager()->GetClientSessionForTesting()) {
    session->StopActorTask(static_cast<int32_t>(task_id),
                           mojom::ActorTaskStopReason::kTaskComplete);
    EXPECT_TRUE(success_future.Wait());
  }
}

// TODO(b/565538504): Re-enable once failure on Android is resolved.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_InvokeTargetLastActiveOrNew_FallbackToNewTab \
  DISABLED_InvokeTargetLastActiveOrNew_FallbackToNewTab
#else
#define MAYBE_InvokeTargetLastActiveOrNew_FallbackToNewTab \
  InvokeTargetLastActiveOrNew_FallbackToNewTab
#endif
IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       MAYBE_InvokeTargetLastActiveOrNew_FallbackToNewTab) {
  auto* tab_list = GetTabListInterface();
  int initial_tab_count = tab_list->GetTabCount();
  BrowserWindowInterface* browser =
      tab_list->GetActiveTab()->GetBrowserWindowInterface();

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);

  // Default to NewConversation which causes getting the last active surface to
  // fail since the instance is new.
  options.target.conversation = glic::NewConversation{};
  options.target.surface =
      glic::LastActiveOrNew{browser, /*open_in_foreground=*/true};
  options.on_success = success_future.GetCallback();

  coordinator().Invoke(std::move(options));

  EXPECT_TRUE(success_future.Wait());

  // A new tab should have been created.
  EXPECT_EQ(tab_list->GetTabCount(), initial_tab_count + 1);

  // The active tab should be a new tab page.
  tabs::TabInterface* active_tab = tab_list->GetActiveTab();
  EXPECT_EQ(active_tab->GetContents()->GetVisibleURL(),
            chrome::ChromeUINewTabURLAsGURL());

  // Glic should be bound to this new tab.
  EXPECT_TRUE(GetInstanceForTab(active_tab));
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeTargetLastActiveOrNew_UsesLastActiveSurface) {
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());

  auto* tab_list = GetTabListInterface();
  int initial_tab_count = tab_list->GetTabCount();
  BrowserWindowInterface* browser = tab1->GetBrowserWindowInterface();

  PreventDeletionOnClose(instance);
  instance->CloseAllEmbedders();
  ASSERT_TRUE(WaitForGlicClose(instance));

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(mojom::InvocationSource::kOsButton);

  options.target.conversation = glic::InstanceId(instance->id());
  options.target.surface =
      glic::LastActiveOrNew{browser, /*open_in_foreground=*/true};
  options.on_success = success_future.GetCallback();

  coordinator().Invoke(std::move(options));

  EXPECT_TRUE(success_future.Wait());

  // No new tab should have been created.
  EXPECT_EQ(tab_list->GetTabCount(), initial_tab_count);
  EXPECT_EQ(tab_list->GetActiveTab(), tab1);

  // Glic should be bound to tab1 again.
  EXPECT_EQ(GetInstanceForTab(tab1), instance);
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest, InvokeFailsWhenClientLoadErrors) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTabAndWaitIdle());
  ASSERT_OK(DisconnectWebClient(instance));

  GlicHistogramTester histogram_tester;
  base::test::TestFuture<GlicInvokeError> error_future;

  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_error = error_future.GetCallback();

  coordinator().Invoke(std::move(options));

  EXPECT_EQ(error_future.Get(), GlicInvokeError::kClientLoadError);
  histogram_tester.ExpectUniqueSample("Glic.InvokeResult",
                                      GlicInvokeError::kClientLoadError, 1);
  histogram_tester.ExpectUniqueSample("Glic.InvokeResult.OsButton",
                                      GlicInvokeError::kClientLoadError, 1);
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeFailsWhenClientLoadErrorsWhileWaiting) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTabAndWaitIdle());
  ASSERT_OK(DisconnectWebClient(instance));
  // The client is disconnected, so the reported failure is what decides
  // between loading and failed. Clear it so the invocation below has to wait.
  instance->host().SetClientLoadFailed(false);
  ASSERT_EQ(instance->host().client_load_state(), ClientLoadState::kLoading);

  GlicHistogramTester histogram_tester;
  base::test::TestFuture<GlicInvokeError> error_future;

  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_error = error_future.GetCallback();

  coordinator().Invoke(std::move(options));

  instance->host().SetClientLoadFailed(true);

  EXPECT_EQ(error_future.Get(), GlicInvokeError::kClientLoadError);
  histogram_tester.ExpectUniqueSample("Glic.InvokeResult",
                                      GlicInvokeError::kClientLoadError, 1);
  histogram_tester.ExpectUniqueSample("Glic.InvokeResult.OsButton",
                                      GlicInvokeError::kClientLoadError, 1);
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeFailsWhenWebClientInitializeFailed) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTabAndWaitIdle());
  ASSERT_OK(DisconnectWebClient(instance));
  instance->host().SetClientLoadFailed(false);
  ASSERT_EQ(instance->host().client_load_state(), ClientLoadState::kLoading);

  GlicHistogramTester histogram_tester;
  base::test::TestFuture<GlicInvokeError> error_future;

  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_error = error_future.GetCallback();

  coordinator().Invoke(std::move(options));

  instance->host().WebClientInitializeFailed();

  EXPECT_EQ(error_future.Get(), GlicInvokeError::kClientLoadError);
  histogram_tester.ExpectUniqueSample("Glic.InvokeResult",
                                      GlicInvokeError::kClientLoadError, 1);
  histogram_tester.ExpectUniqueSample("Glic.InvokeResult.OsButton",
                                      GlicInvokeError::kClientLoadError, 1);
}

IN_PROC_BROWSER_TEST_F(GlicInvokeBrowserTest,
                       InvokeSucceedsWhenClientAlreadyReady) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK(OpenGlicForActiveTabAndWaitIdle());

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();

  coordinator().Invoke(std::move(options));

  EXPECT_TRUE(success_future.Wait());
}

// Delays the mock client's registration so that an invocation is guaranteed to
// start while the client is still loading.
class GlicInvokeSlowClientBrowserTest : public GlicInvokeBrowserTest {
 public:
  GlicInvokeSlowClientBrowserTest() {
    AddMockGlicQueryParam("delay_ms", "2000");
  }
};

IN_PROC_BROWSER_TEST_F(GlicInvokeSlowClientBrowserTest,
                       InvokeSucceedsWhenClientTransitionsToReady) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_EQ(instance->host().client_load_state(), ClientLoadState::kLoading);

  base::test::TestFuture<void> success_future;
  GlicInvokeOptions options(glic::Target(*tab),
                            mojom::InvocationSource::kOsButton);
  options.on_success = success_future.GetCallback();

  coordinator().Invoke(std::move(options));

  // The invocation waits for the client to finish loading before succeeding.
  EXPECT_TRUE(success_future.Wait());
  EXPECT_EQ(instance->host().client_load_state(), ClientLoadState::kReady);
}

}  // namespace glic
