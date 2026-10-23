// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_keyed_service.h"

#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/dictation/dictation_browser_test_base.h"
#include "chrome/browser/dictation/dictation_keyed_service_factory.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/dictation/metrics.h"
#include "chrome/browser/dictation/stream_provider.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/glic/browser_ui/tab_underline_view.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/dictation/onboarding_dialog_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/common/extensions/api/dictation_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/global_dom_node_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/blink/public/common/dom/dom_node_id.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

namespace dictation {

namespace {

#define EXPECT_EDITABLE_TEXT_EQ(selector, expected_text) \
  EXPECT_EQ(expected_text, GetEditableExpectedText(selector, expected_text));

using ExtensionStreamState = extensions::api::dictation_private::StreamState;
using ExtensionTranscriptionType =
    extensions::api::dictation_private::TranscriptionType;

class DictationKeyedServiceBrowserTest
    : public DictationBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  DictationKeyedServiceBrowserTest()
      : DictationKeyedServiceBrowserTest(GetParam()) {}
  explicit DictationKeyedServiceBrowserTest(bool session_ends_on_stream_end)
      : DictationBrowserTestBase(session_ends_on_stream_end) {}
  ~DictationKeyedServiceBrowserTest() override = default;

  void SimulateSpeechRecognition(ListenerStreamProvider* provider,
                                 ExtensionTranscriptionType type,
                                 std::string_view text) {
    ExtensionSendTranscriptUpdate(profile(), provider->stream_id_for_testing(),
                                  type, text);
  }

  std::string GetEditableExpectedText(std::string_view selector,
                                      std::string_view expected) {
    return dictation::GetEditableExpectedText(
        web_contents()->GetPrimaryMainFrame(), selector, expected);
  }
};

IN_PROC_BROWSER_TEST_P(DictationKeyedServiceBrowserTest,
                       CreatedForRegularProfile) {
  EXPECT_NE(DictationKeyedService::Get(profile()), nullptr);
}

IN_PROC_BROWSER_TEST_P(DictationKeyedServiceBrowserTest,
                       NotCreatedForOTRProfile) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_EQ(DictationKeyedService::Get(otr_profile), nullptr);
}

class DictationKeyedServiceDisabledBrowserTest
    : public DictationKeyedServiceBrowserTest {
 public:
  DictationKeyedServiceDisabledBrowserTest()
      : DictationKeyedServiceBrowserTest(
            kSessionEndsOnStreamEnd.default_value) {
    scoped_feature_list_.InitAndDisableFeature(kDictation);
  }
  ~DictationKeyedServiceDisabledBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceDisabledBrowserTest,
                       NotCreatedWhenDisabled) {
  EXPECT_EQ(DictationKeyedService::Get(profile()), nullptr);
}

// Ensure the context menu entrypoint is shown both before, during, and after a
// session is active.
IN_PROC_BROWSER_TEST_P(DictationKeyedServiceBrowserTest,
                       ShouldShowContextMenuItem) {
  EXPECT_TRUE(dictation_service().ShouldShowContextMenuItem());

  StartSession();

  EXPECT_TRUE(dictation_service().ShouldShowContextMenuItem());

  dictation_service().EndSession();

  EXPECT_TRUE(dictation_service().ShouldShowContextMenuItem());
}

// Talk to Type is elided when right-clicking a misspelled word, since that menu
// is already long.
IN_PROC_BROWSER_TEST_P(DictationKeyedServiceBrowserTest,
                       ContextMenuHiddenForMisspelledWord) {
  content::ContextMenuParams params;
  params.is_editable = true;
  params.misspelled_word = u"helo";
  params.dictionary_suggestions = {u"hello"};
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.Init();

  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_DICTATION));
}

IN_PROC_BROWSER_TEST_P(DictationKeyedServiceBrowserTest,
                       ExecuteContextMenuCommand) {
  content::ContextMenuParams params;
  params.is_editable = true;
  params.form_field_dom_node_id = content::GlobalDOMNodeId(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr(),
      blink::DOMNodeIdType(123));
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.Init();

  ASSERT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_DICTATION));
  ASSERT_TRUE(menu.IsItemEnabled(IDC_CONTENT_CONTEXT_DICTATION));

  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_DICTATION, 0);

  ASSERT_NE(session_controller(), nullptr);
  StreamProvider* provider = session_controller()->attached_stream_provider();
  ASSERT_NE(provider, nullptr);
  ASSERT_NE(provider->GetTarget(), nullptr);
  EXPECT_EQ(provider->GetTarget()->global_dom_node_id().target_element_dom_id,
            blink::DOMNodeIdType(123));
  EXPECT_FALSE(provider->GetTarget()->richly_editable());
}

// Ensure the context menu item can be used to start a new stream in the same
// tab as an existing session.
IN_PROC_BROWSER_TEST_P(DictationKeyedServiceBrowserTest,
                       ExecuteContextMenuCommandExistingSessionSameTab) {
  // Start a first stream
  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(123));

  ASSERT_NE(session_controller(), nullptr);
  ListenerStreamProvider* stream1_provider = attached_stream();
  auto stream1_id = stream1_provider->stream_id_for_testing();
  ExtensionWaitForStreamStart(profile(), stream1_id);
  ExtensionSendStreamStateUpdate(
      profile(), stream1_id,
      extensions::api::dictation_private::StreamState::kTranscribing);

  // Now that a session and stream are active, verify that we can still trigger
  // dictation from the context menu. This should gracefully end and finalize
  // the first stream and start a new stream.
  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(456));

  ExtensionWaitForStreamEnd(profile(), stream1_id);
  ASSERT_NE(attached_stream(), nullptr);
  auto stream2_id = attached_stream()->stream_id_for_testing();
  EXPECT_NE(stream1_id, stream2_id);
  EXPECT_EQ(attached_stream()
                ->GetTarget()
                ->global_dom_node_id()
                .target_element_dom_id,
            blink::DOMNodeIdType(456));

  // Ensure the finalized transcript can be sent for stream 1 after stream 2
  // started.
  ExtensionSendTranscriptUpdate(
      profile(), stream1_id,
      extensions::api::dictation_private::TranscriptionType::kFinal, "Final");
  EXPECT_EQ(stream1_provider->GetLatestTranscriptionForTesting(), "Final");
}

// Ensure the context menu item can be used to start a new stream in a second
// window, while a session is already active in another window.
IN_PROC_BROWSER_TEST_P(
    DictationKeyedServiceBrowserTest,
    ExecuteContextMenuCommandExistingSessionDifferentWindow) {
  // Start dictation in the first window.
  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(123));
  ListenerStreamProvider* stream1_provider = attached_stream();
  auto stream1_id = stream1_provider->stream_id_for_testing();
  ExtensionWaitForStreamStart(profile(), stream1_id);
  ExtensionSendStreamStateUpdate(
      profile(), stream1_id,
      extensions::api::dictation_private::StreamState::kTranscribing);

  // Create a second window and trigger the context menu entry point from it.
  BrowserWindowInterface* second_browser = CreateBrowser(profile());
  content::WebContents* window2_contents =
      second_browser->GetTabStripModel()->GetActiveWebContents();
  SimulateInvokeViaContextMenu(window2_contents->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(456));

  // Ensure stream 1 had EndStream called on it.
  ExtensionWaitForStreamEnd(profile(), stream1_id);

  // The session should should now be targeting the new element in the second
  // window.
  ASSERT_NE(attached_stream(), nullptr);
  auto stream2_id = attached_stream()->stream_id_for_testing();
  EXPECT_NE(stream1_id, stream2_id);
  EXPECT_EQ(attached_stream()
                ->GetTarget()
                ->global_dom_node_id()
                .target_element_dom_id,
            blink::DOMNodeIdType(456));
  EXPECT_EQ(attached_stream()
                ->GetTarget()
                ->global_dom_node_id()
                .document.AsRenderFrameHostIfValid(),
            window2_contents->GetPrimaryMainFrame());

  // Ensure final transcript can be sent for stream 1 after stream 2 started.
  ExtensionSendTranscriptUpdate(
      profile(), stream1_id,
      extensions::api::dictation_private::TranscriptionType::kFinal, "Final");
  EXPECT_EQ(stream1_provider->GetLatestTranscriptionForTesting(), "Final");
}

IN_PROC_BROWSER_TEST_P(DictationKeyedServiceBrowserTest,
                       ExecuteContextMenuCommandRichlyEditable) {
  content::ContextMenuParams params;
  params.is_editable = true;
  params.edit_flags = blink::ContextMenuDataEditFlags::kCanEditRichly;
  params.form_field_dom_node_id = content::GlobalDOMNodeId(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr(),
      blink::DOMNodeIdType(123));
  TestRenderViewContextMenu menu(*web_contents()->GetPrimaryMainFrame(),
                                 params);
  menu.Init();

  ASSERT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_DICTATION));
  ASSERT_TRUE(menu.IsItemEnabled(IDC_CONTENT_CONTEXT_DICTATION));

  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_DICTATION, 0);

  ASSERT_NE(session_controller(), nullptr);
  StreamProvider* provider = session_controller()->attached_stream_provider();
  ASSERT_NE(provider, nullptr);
  ASSERT_NE(provider->GetTarget(), nullptr);
  EXPECT_EQ(provider->GetTarget()->global_dom_node_id().target_element_dom_id,
            blink::DOMNodeIdType(123));
  EXPECT_TRUE(provider->GetTarget()->richly_editable());
}

// Not parameterized: password-field handling does not depend on
// `kSessionEndsOnStreamEnd`.
class DictationPasswordLikeFieldBrowserTest
    : public DictationKeyedServiceBrowserTest {
 public:
  DictationPasswordLikeFieldBrowserTest()
      : DictationKeyedServiceBrowserTest(
            kSessionEndsOnStreamEnd.default_value) {}

 protected:
  void SetUpOnMainThread() override {
    DictationKeyedServiceBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(content::NavigateToURL(
        web_contents(),
        embedded_test_server()->GetURL("/textinput/all_input_node.html")));
    // Turn the contenteditable into a CSS-masked custom password field.
    ASSERT_TRUE(content::ExecJs(web_contents(), R"(
        const editable = document.getElementById('contenteditable_id');
        editable.style.webkitTextSecurity = 'disc';
        editable.textContent = 'secret';
    )"));
  }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  blink::DOMNodeIdType NodeId(std::string_view selector) {
    return blink::DOMNodeIdType(
        content::GetDOMNodeId(*main_frame(), selector).value());
  }

  // Focuses `selector` and waits until the browser knows it is focused. If
  // `password_like`, also waits for the text input state identifying it as a
  // password-like field.
  void FocusAndWait(std::string_view selector, bool password_like) {
    ASSERT_TRUE(content::ExecJs(
        web_contents(),
        content::JsReplace("document.querySelector($1).focus();", selector)));
    blink::DOMNodeIdType expected = NodeId(selector);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return main_frame()->GetFocusedDOMNodeId() == expected &&
             (!password_like || main_frame()->IsFocusedElementPasswordLike());
    })) << selector;
  }

  bool IsContextMenuItemPresent(std::string_view selector) {
    content::ContextMenuParams params;
    params.is_editable = true;
    params.form_field_dom_node_id = content::GlobalDOMNodeId(
        main_frame()->GetWeakDocumentPtr(), NodeId(selector));
    TestRenderViewContextMenu menu(*main_frame(), params);
    menu.Init();
    return menu.IsItemPresent(IDC_CONTENT_CONTEXT_DICTATION);
  }
};

IN_PROC_BROWSER_TEST_F(DictationPasswordLikeFieldBrowserTest,
                       ContextMenuHiddenForPasswordLikeFields) {
  FocusAndWait("#password_id", /*password_like=*/true);
  EXPECT_FALSE(IsContextMenuItemPresent("#password_id"));

  FocusAndWait("#contenteditable_id", /*password_like=*/true);
  EXPECT_FALSE(IsContextMenuItemPresent("#contenteditable_id"));

  FocusAndWait("#text_id", /*password_like=*/false);
  EXPECT_TRUE(IsContextMenuItemPresent("#text_id"));
}

IN_PROC_BROWSER_TEST_F(DictationPasswordLikeFieldBrowserTest,
                       HotkeyIgnoredInPasswordLikeField) {
  FocusAndWait("#password_id", /*password_like=*/true);
  dictation_service().ToggleHotkeyHandler();
  EXPECT_EQ(session_controller(), nullptr);

  FocusAndWait("#text_id", /*password_like=*/false);
  dictation_service().ToggleHotkeyHandler();
  ASSERT_NE(session_controller(), nullptr);
  EXPECT_NE(session_controller()->attached_stream_provider(), nullptr);
  dictation_service().EndSession();
}

// TODO(crbug.com/502587072): Add tests which have the test extension simulate
// stream failures, including on start and mid stream.

IN_PROC_BROWSER_TEST_P(DictationKeyedServiceBrowserTest,
                       StartSessionAndReceiveTranscription) {
  StartSession();

  SessionController* controller = session_controller();
  ASSERT_NE(controller, nullptr);

  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      controller->attached_stream_provider());
  ASSERT_NE(provider, nullptr);

  ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());

  ExtensionSendStreamStateUpdate(profile(), provider->stream_id_for_testing(),
                                 ExtensionStreamState::kTranscribing);
  EXPECT_EQ(provider->GetState(), StreamProvider::StreamState::kTranscribing);

  // Send partial transcript.
  ExtensionSendTranscriptUpdate(profile(), provider->stream_id_for_testing(),
                                ExtensionTranscriptionType::kPartial, "Hello");
  EXPECT_EQ(provider->GetLatestTranscriptionForTesting(), "Hello");
  EXPECT_FALSE(provider->IsTranscriptionFinalForTesting());

  // Send final transcript.
  ExtensionSendTranscriptUpdate(profile(), provider->stream_id_for_testing(),
                                ExtensionTranscriptionType::kFinal,
                                "Hello world");
  EXPECT_EQ(provider->GetLatestTranscriptionForTesting(), "Hello world");
  EXPECT_TRUE(provider->IsTranscriptionFinalForTesting());

  // Stop the provider from the browser side and confirm the state change from
  // the extension API.
  provider->Stop(DictationStreamEndTrigger::kTest);
  ExtensionSendStreamStateUpdate(profile(), provider->stream_id_for_testing(),
                                 ExtensionStreamState::kComplete);
  WaitForSessionState(SessionState::kInactive);
}

IN_PROC_BROWSER_TEST_P(DictationKeyedServiceBrowserTest,
                       EndActiveStreamEntersFinalizingState) {
  StartSession();

  SessionController* controller = session_controller();
  ASSERT_NE(controller, nullptr);

  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      controller->attached_stream_provider());
  ASSERT_NE(provider, nullptr);

  ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());

  ExtensionSendStreamStateUpdate(profile(), provider->stream_id_for_testing(),
                                 ExtensionStreamState::kTranscribing);
  ASSERT_EQ(controller->GetState(), SessionState::kTranscribing);

  // Send a transcription update ("Hello") as a partial update and wait for it
  // to be received.
  SimulateSpeechRecognition(provider, ExtensionTranscriptionType::kPartial,
                            "Hello");

  ASSERT_EQ(provider->GetLatestTranscriptionForTesting(), "Hello");
  ASSERT_FALSE(provider->IsTranscriptionFinalForTesting());

  // Simulate the stream being ended.
  controller->UiRequestEndActiveStream();

  // Ensure the controller enters kFinalizing state.
  EXPECT_EQ(controller->GetState(), SessionState::kFinalizing);

  // The finalizing provider should still be able to send a final transcription
  // update.
  SimulateSpeechRecognition(provider, ExtensionTranscriptionType::kFinal,
                            "Hello World");
  EXPECT_EQ(provider->GetLatestTranscriptionForTesting(), "Hello World");
  EXPECT_TRUE(provider->IsTranscriptionFinalForTesting());

  // TODO(b/508729855) Ensure a final transcript update when finalizing gets
  // committed to the target.
}

IN_PROC_BROWSER_TEST_P(DictationKeyedServiceBrowserTest,
                       StartNewStreamWhileFinalizing) {
  StartSession();

  SessionController* controller = session_controller();
  ASSERT_NE(controller, nullptr);

  ListenerStreamProvider* provider1 = static_cast<ListenerStreamProvider*>(
      controller->attached_stream_provider());
  ASSERT_NE(provider1, nullptr);

  ExtensionWaitForStreamStart(profile(), provider1->stream_id_for_testing());

  // Wait for the first stream to transition to transcribing.
  ExtensionSendStreamStateUpdate(profile(), provider1->stream_id_for_testing(),
                                 ExtensionStreamState::kTranscribing);
  ASSERT_EQ(controller->GetState(), SessionState::kTranscribing);

  SimulateSpeechRecognition(provider1, ExtensionTranscriptionType::kPartial,
                            "Hello");

  // Put the first stream into finalization.
  controller->UiRequestEndActiveStream();

  // The first stream is now in the finalizing set, and the controller is in
  // kFinalizing.
  ASSERT_EQ(controller->GetState(), SessionState::kFinalizing);
  ASSERT_EQ(controller->attached_stream_provider(), nullptr);

  // Start a second stream while the first is finalizing. The controller should
  // immediately enter kStreamInitializing.
  controller->StartDictationStream(DefaultInPageTarget(web_contents()),
                                   DictationStreamStartTrigger::kSessionStart,
                                   base::TimeTicks::Now());
  EXPECT_EQ(controller->GetState(), SessionState::kStreamInitializing);

  // Wait for the stream to enter transcribing state.
  ListenerStreamProvider* provider2 = static_cast<ListenerStreamProvider*>(
      controller->attached_stream_provider());
  ExtensionWaitForStreamStart(profile(), provider2->stream_id_for_testing());
  ExtensionSendStreamStateUpdate(profile(), provider2->stream_id_for_testing(),
                                 ExtensionStreamState::kTranscribing);
  ASSERT_EQ(controller->GetState(), SessionState::kTranscribing);

  // Recognition from the new provider arrives.
  SimulateSpeechRecognition(provider2, ExtensionTranscriptionType::kPartial,
                            "World");
  EXPECT_EQ(provider2->GetLatestTranscriptionForTesting(), "World");
}

IN_PROC_BROWSER_TEST_P(DictationKeyedServiceBrowserTest,
                       ProviderDestroyedAfterComplete) {
  StartSession();

  SessionController* controller = session_controller();
  ASSERT_NE(controller, nullptr);

  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      controller->attached_stream_provider());
  ASSERT_NE(provider, nullptr);

  ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());

  base::WeakPtr<ListenerStreamProvider> provider_weak = provider->GetWeakPtr();
  ASSERT_NE(provider_weak, nullptr);

  // Transition to transcribing.
  ExtensionSendStreamStateUpdate(profile(), provider->stream_id_for_testing(),
                                 ExtensionStreamState::kTranscribing);
  EXPECT_EQ(provider->GetState(), StreamProvider::StreamState::kTranscribing);

  // Stop the provider and confirm the state change from the extension. This
  // should trigger a deletion task.
  provider->Stop(DictationStreamEndTrigger::kTest);
  ExtensionSendStreamStateUpdate(profile(), provider->stream_id_for_testing(),
                                 ExtensionStreamState::kComplete);
  EXPECT_TRUE(base::test::RunUntil([&]() { return provider_weak == nullptr; }));
}

IN_PROC_BROWSER_TEST_P(DictationKeyedServiceBrowserTest,
                       ProviderDestroyedAfterFailed) {
  StartSession();

  SessionController* controller = session_controller();
  ASSERT_NE(controller, nullptr);

  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      controller->attached_stream_provider());
  ASSERT_NE(provider, nullptr);

  ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());

  base::WeakPtr<ListenerStreamProvider> provider_weak = provider->GetWeakPtr();
  ASSERT_NE(provider_weak, nullptr);

  // Transition to transcribing.
  ExtensionSendStreamStateUpdate(profile(), provider->stream_id_for_testing(),
                                 ExtensionStreamState::kTranscribing);
  EXPECT_EQ(provider->GetState(), StreamProvider::StreamState::kTranscribing);

  // Simulate a stream failure from the extension. This should trigger a
  // deletion task.
  ExtensionSendStreamStateUpdate(profile(), provider->stream_id_for_testing(),
                                 ExtensionStreamState::kFailed);
  EXPECT_TRUE(base::test::RunUntil([&]() { return provider_weak == nullptr; }));
}

IN_PROC_BROWSER_TEST_P(DictationKeyedServiceBrowserTest,
                       TranscriptionCommittedToElement) {
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(web_contents());
  content::MainThreadFrameObserver frame_observer(
      web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
  frame_observer.Wait();

  // Focus the textarea so that dictation targets it.
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "text_id");

  std::optional<int> dom_node_id =
      content::GetDOMNodeId(*web_contents()->GetPrimaryMainFrame(), "#text_id");
  ASSERT_TRUE(dom_node_id.has_value());

  StartSession(TargetDetails(content::GlobalDOMNodeId{
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr(),
      blink::DOMNodeIdType(dom_node_id.value())}));

  SessionController* controller = session_controller();
  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      controller->attached_stream_provider());

  ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());

  ExtensionSendStreamStateUpdate(profile(), provider->stream_id_for_testing(),
                                 ExtensionStreamState::kTranscribing);

  SimulateSpeechRecognition(provider, ExtensionTranscriptionType::kPartial,
                            "Hello");
  SimulateSpeechRecognition(provider, ExtensionTranscriptionType::kFinal,
                            "Hello World");
  EXPECT_EQ(provider->GetLatestTranscriptionForTesting(), "Hello World");
  EXPECT_TRUE(provider->IsTranscriptionFinalForTesting());

  provider->Stop(DictationStreamEndTrigger::kTest);
  ExtensionSendStreamStateUpdate(profile(), provider->stream_id_for_testing(),
                                 ExtensionStreamState::kComplete);

  // Verify the transcription reached the document.
  EXPECT_EDITABLE_TEXT_EQ("#text_id", "Hello World");
}

IN_PROC_BROWSER_TEST_P(DictationKeyedServiceBrowserTest,
                       ToggleStreamAndCommit) {
  if (GetParam()) {
    GTEST_SKIP()
        << "Multiple streams per session are not possible in this config.";
  }

  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(web_contents());
  content::MainThreadFrameObserver frame_observer(
      web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
  frame_observer.Wait();

  // Focus the textarea so that dictation targets it.
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "text_id");

  std::optional<int> dom_node_id =
      content::GetDOMNodeId(*web_contents()->GetPrimaryMainFrame(), "#text_id");
  ASSERT_TRUE(dom_node_id.has_value());

  // Start a new session and stream, commit some text, and stop.
  {
    StartSession(TargetDetails(content::GlobalDOMNodeId{
        web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr(),
        blink::DOMNodeIdType(dom_node_id.value())}));

    ASSERT_TRUE(attached_stream());
    auto stream_id = attached_stream()->stream_id_for_testing();

    ExtensionWaitForStreamStart(profile(), stream_id);
    ExtensionSendStreamStateUpdate(profile(), stream_id,
                                   ExtensionStreamState::kTranscribing);

    SimulateSpeechRecognition(attached_stream(),
                              ExtensionTranscriptionType::kFinal, "Hello");

    session_controller()->UiRequestEndActiveStream();
    ExtensionSendStreamStateUpdate(profile(), stream_id,
                                   ExtensionStreamState::kComplete);
  }

  EXPECT_EDITABLE_TEXT_EQ("#text_id", "Hello");
  ASSERT_FALSE(attached_stream());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return session_controller()->GetState() == SessionState::kInactive;
  }));

  // Start a second stream simulating a click on the "Start" button.
  {
    session_controller()->UiRequestStartStream();
    ASSERT_TRUE(attached_stream());

    auto stream_id = attached_stream()->stream_id_for_testing();

    ExtensionWaitForStreamStart(profile(), stream_id);
    ExtensionSendStreamStateUpdate(profile(), stream_id,
                                   ExtensionStreamState::kTranscribing);

    SimulateSpeechRecognition(attached_stream(),
                              ExtensionTranscriptionType::kFinal, " World");

    session_controller()->UiRequestEndActiveStream();
    ExtensionSendStreamStateUpdate(profile(), stream_id,
                                   ExtensionStreamState::kComplete);
  }

  EXPECT_EDITABLE_TEXT_EQ("#text_id", "Hello World");
}

IN_PROC_BROWSER_TEST_P(DictationKeyedServiceBrowserTest,
                       TypeIntoEditableEndsStream) {
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(web_contents());
  content::MainThreadFrameObserver frame_observer(
      web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
  frame_observer.Wait();

  // Focus the textarea so that keyboard input and dictation target it.
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "text_id");

  // Type an initial character into the textarea.
  content::SimulateKeyPress(web_contents(), ui::DomKey::FromCharacter('a'),
                            ui::DomCode::US_A, ui::VKEY_A, false, false, false,
                            false);
  EXPECT_EDITABLE_TEXT_EQ("#text_id", "a");

  std::optional<int> dom_node_id =
      content::GetDOMNodeId(*web_contents()->GetPrimaryMainFrame(), "#text_id");
  ASSERT_TRUE(dom_node_id.has_value());

  // Start a dictation session for the textarea.
  StartSession(TargetDetails(content::GlobalDOMNodeId{
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr(),
      blink::DOMNodeIdType(dom_node_id.value())}));

  SessionController* controller = session_controller();
  ASSERT_NE(controller, nullptr);
  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      controller->attached_stream_provider());
  ASSERT_NE(provider, nullptr);
  auto stream_id = provider->stream_id_for_testing();

  ExtensionWaitForStreamStart(profile(), stream_id);
  ExtensionSendStreamStateUpdate(profile(), stream_id,
                                 ExtensionStreamState::kTranscribing);
  ASSERT_EQ(controller->GetState(), SessionState::kTranscribing);

  // While transcribing, type an additional character.
  content::SimulateKeyPress(web_contents(), ui::DomKey::FromCharacter('b'),
                            ui::DomCode::US_B, ui::VKEY_B, false, false, false,
                            false);

  // Verify that the user typing in the textarea causes the stream to finalize.
  WaitForSessionState(SessionState::kFinalizing);
  EXPECT_EQ(controller->attached_stream_provider(), nullptr);

  ExtensionWaitForStreamEnd(profile(), stream_id);
  SimulateSpeechRecognition(provider, ExtensionTranscriptionType::kFinal, "c");
  ExtensionSendStreamStateUpdate(profile(), stream_id,
                                 ExtensionStreamState::kComplete);

  WaitForSessionState(SessionState::kInactive);

  // Verify the resulting text in the textarea is the initial character,
  // followed by the additional typed character, followed by the transcription
  // (separated by a space due to whitespace insertion behavior).
  EXPECT_EDITABLE_TEXT_EQ("#text_id", "ab c");
}

INSTANTIATE_TEST_SUITE_P(All,
                         DictationKeyedServiceBrowserTest,
                         testing::Bool());

// TODO(b/533465625): Ideally we could also make this a child of
// DictationBrowserTestBase so we get all the helpers.
class DictationGlicBrowserTest : public glic::GlicBrowserTest,
                                 public testing::WithParamInterface<bool> {
 public:
  DictationGlicBrowserTest()
      : scoped_feature_list_(CreateEnablingFeatureList()) {
    glic_feature_list_.InitWithFeatureStates(
        {{::features::kGlicNoWebview, GetParam()},
         {::features::kGlicCaptureRegion, true}});
  }
  ~DictationGlicBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    glic::GlicBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
        std::string(kDictationTestExtensionId));
  }

  void SetUpOnMainThread() override {
    glic::GlicBrowserTest::SetUpOnMainThread();
    GetProfile()->GetPrefs()->SetBoolean(
        prefs::kPrefDictationOnboardingCompleted, true);
    LoadTestExtensionInManualMode(GetProfile());
  }

  DictationKeyedService& dictation_service() {
    return *DictationKeyedService::Get(GetProfile());
  }

  glic::TabUnderlineView* GetUnderlineView(tabs::TabInterface* tab) {
    auto* browser_view =
        BrowserView::GetBrowserViewForBrowser(tab->GetBrowserWindowInterface());
    auto* tab_view =
        browser_view->tab_strip_view()->GetTabAnchorView(tab->GetHandle());
    if (!tab_view) {
      return nullptr;
    }
    views::View* view = tab_view->GetViewByElementId(
        glic::TabUnderlineView::kGlicTabUnderlineElementId);
    return views::AsViewClass<glic::TabUnderlineView>(view);
  }

  // Creates and focuses an input element in `rfh`, triggers the dictation
  // hotkey, and verifies the resulting stream targets that input element.
  void FocusInputAndVerifyHotkeyTargets(content::RenderFrameHost* rfh) {
    auto* contents = content::WebContents::FromRenderFrameHost(rfh);
    ASSERT_TRUE(contents);
    contents->Focus();
    content::FocusWebContentsOnFrame(contents, rfh);
    ASSERT_TRUE(content::ExecJs(rfh, R"(
      const input = document.createElement('input');
      input.id = 'test_input';
      document.body.appendChild(input);
      input.focus();
    )"));
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return !rfh->GetFocusedDOMNodeId().is_null(); }));

    std::optional<int> expected_node_id =
        content::GetDOMNodeId(*rfh, "#test_input");
    ASSERT_TRUE(expected_node_id.has_value());

    dictation_service().ToggleHotkeyHandler();

    SessionController* controller = dictation_service().session_controller();
    ASSERT_NE(controller, nullptr);
    StreamProvider* provider = controller->attached_stream_provider();
    ASSERT_NE(provider, nullptr);
    ASSERT_NE(provider->GetTarget(), nullptr);
    EXPECT_EQ(provider->GetTarget()
                  ->global_dom_node_id()
                  .document.AsRenderFrameHostIfValid(),
              rfh);
    EXPECT_EQ(provider->GetTarget()->global_dom_node_id().target_element_dom_id,
              blink::DOMNodeIdType(*expected_node_id));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedFeatureList glic_feature_list_;
};

// Ensure basic stream setup, state changes, and end work correctly for streams
// started for a Glic guest.
IN_PROC_BROWSER_TEST_P(DictationGlicBrowserTest, BasicStreamFunctions) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::RenderFrameHost* glic_rfh = instance->host().GetGuestMainFrame();
  ASSERT_TRUE(glic_rfh);

  // Start a session using the Glic guest document rather than the normal tab
  // document.
  content::GlobalDOMNodeId target_id(glic_rfh->GetWeakDocumentPtr(),
                                     blink::DOMNodeIdType(123));

  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab);
  dictation_service().StartSessionForTesting(
      *tab, TargetDetails(target_id), DictationSessionEntryPoint::kContextMenu);

  SessionController* controller = dictation_service().session_controller();
  ASSERT_NE(controller, nullptr);

  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      controller->attached_stream_provider());
  ASSERT_NE(provider, nullptr);

  ExtensionWaitForStreamStart(GetProfile(), provider->stream_id_for_testing());

  ExtensionSendStreamStateUpdate(GetProfile(),
                                 provider->stream_id_for_testing(),
                                 ExtensionStreamState::kTranscribing);
  EXPECT_EQ(provider->GetState(), StreamProvider::StreamState::kTranscribing);

  ExtensionSendTranscriptUpdate(GetProfile(), provider->stream_id_for_testing(),
                                ExtensionTranscriptionType::kPartial, "Hello");
  EXPECT_EQ(provider->GetLatestTranscriptionForTesting(), "Hello");

  controller->UiRequestEndActiveStream();
  ExtensionSendStreamStateUpdate(GetProfile(),
                                 provider->stream_id_for_testing(),
                                 ExtensionStreamState::kComplete);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !dictation_service().session_controller() ||
           dictation_service().session_controller()->GetState() ==
               SessionState::kInactive;
  }));

  dictation_service().EndSession();
}

IN_PROC_BROWSER_TEST_P(DictationGlicBrowserTest, TabUnderlineVisibility) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab);

  auto* underline = GetUnderlineView(tab);
  ASSERT_TRUE(underline);
  EXPECT_FALSE(underline->IsShowing());

  // Start dictation session on the active tab.
  content::GlobalDOMNodeId target_id(
      tab->GetContents()->GetPrimaryMainFrame()->GetWeakDocumentPtr(),
      blink::DOMNodeIdType(123));

  dictation_service().StartSessionForTesting(
      *tab, TargetDetails(target_id), DictationSessionEntryPoint::kContextMenu);

  // The underline should become visible.
  EXPECT_TRUE(underline->IsShowing());

  // End session.
  dictation_service().EndSession();

  EXPECT_FALSE(underline->IsShowing());
}

IN_PROC_BROWSER_TEST_P(DictationGlicBrowserTest,
                       TabUnderlineHiddenOnNavigation) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab);

  auto* underline = GetUnderlineView(tab);
  ASSERT_TRUE(underline);
  EXPECT_FALSE(underline->IsShowing());

  // Start dictation session on the active tab.
  content::GlobalDOMNodeId target_id(
      tab->GetContents()->GetPrimaryMainFrame()->GetWeakDocumentPtr(),
      blink::DOMNodeIdType(123));

  dictation_service().StartSessionForTesting(
      *tab, TargetDetails(target_id), DictationSessionEntryPoint::kContextMenu);

  EXPECT_TRUE(underline->IsShowing());

  // Navigate the active tab.
  ASSERT_TRUE(
      content::NavigateToURL(tab->GetContents(), GURL("chrome://version")));

  // The underline should become hidden because navigation ends dictation.
  EXPECT_FALSE(underline->IsShowing());
}

// Ensure session URL category metric is recorded as Glic for sessions started
// in Glic guest.
IN_PROC_BROWSER_TEST_P(DictationGlicBrowserTest,
                       RecordsSessionUrlCategoryGlic) {
  base::HistogramTester histogram_tester;

  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::RenderFrameHost* glic_rfh = instance->host().GetGuestMainFrame();
  ASSERT_TRUE(glic_rfh);

  // Start a session using the Glic guest document.
  content::GlobalDOMNodeId target_id(glic_rfh->GetWeakDocumentPtr(),
                                     blink::DOMNodeIdType(123));

  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab);
  dictation_service().StartSessionForTesting(
      *tab, TargetDetails(target_id), DictationSessionEntryPoint::kContextMenu);

  histogram_tester.ExpectUniqueSample(kSessionUrlCategoryHistogramName,
                                      DictationUrlCategory::kGlic, 1);

  dictation_service().EndSession();
}

IN_PROC_BROWSER_TEST_P(DictationGlicBrowserTest,
                       ToggleHotkeyTargetsFocusedGlicSidePanel) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab);
  content::WebContents* tab_contents = tab->GetContents();
  ASSERT_TRUE(tab_contents);

  // Navigate the main tab and focus its textarea first so the web page has a
  // focused editable node.
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");
  ASSERT_TRUE(content::NavigateToURL(tab_contents, url));
  ASSERT_TRUE(content::ExecJs(tab_contents,
                              "document.getElementById('text_id').focus();"));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    auto* frame = tab_contents->GetFocusedFrame();
    return frame && !frame->GetFocusedDOMNodeId().is_null();
  }));

  // Open the Glic side panel and focus an input element inside the Glic guest.
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::RenderFrameHost* glic_rfh = instance->host().GetGuestMainFrame();
  ASSERT_TRUE(glic_rfh);

  // Triggering the dictation hotkey should target the focused Glic side panel
  // input box rather than the main tab's input box.
  FocusInputAndVerifyHotkeyTargets(glic_rfh);

  // Pressing the hotkey again should stop the active stream in Glic.
  dictation_service().ToggleHotkeyHandler();
  ASSERT_NE(dictation_service().session_controller(), nullptr);
  EXPECT_EQ(
      dictation_service().session_controller()->attached_stream_provider(),
      nullptr);

  dictation_service().EndSession();
}

class DictationGlicSelectionOverlayBrowserTest
    : public DictationGlicBrowserTest {};

IN_PROC_BROWSER_TEST_P(DictationGlicSelectionOverlayBrowserTest,
                       SelectionOverlaySupport) {
  base::HistogramTester histogram_tester;

  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_TRUE(tab);
  content::WebContents* tab_contents = tab->GetContents();
  ASSERT_TRUE(tab_contents);

  auto* overlay_controller =
      glic::SelectionOverlayController::FromTabWebContents(tab_contents);
  ASSERT_TRUE(overlay_controller);
  overlay_controller->Show(/*options=*/nullptr);

  ASSERT_OK(glic::RunUntilEqual(
      [&]() { return overlay_controller->state(); },
      glic::SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));

  content::RenderFrameHost* overlay_rfh =
      overlay_controller->GetOverlayMainFrame();
  ASSERT_TRUE(overlay_rfh);

  // Triggering via context menu on the selection overlay should resolve the
  // host tab and record the WebUI URL category.
  content::GlobalDOMNodeId context_menu_target(
      overlay_rfh->GetWeakDocumentPtr(), blink::DOMNodeIdType(123));
  dictation_service().ContextMenuHandler(TargetDetails(context_menu_target));

  SessionController* controller = dictation_service().session_controller();
  ASSERT_NE(controller, nullptr);
  EXPECT_EQ(dictation_service().GetActiveDictationTab(), tab);
  histogram_tester.ExpectUniqueSample(kSessionUrlCategoryHistogramName,
                                      DictationUrlCategory::kWebUI, 1);
  dictation_service().EndSession();

  // Focus an input inside the selection overlay and verify the hotkey targets
  // it.
  FocusInputAndVerifyHotkeyTargets(overlay_rfh);

  // Closing the selection overlay destroys its WebContents and should end the
  // active dictation session.
  overlay_controller->Close();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return dictation_service().session_controller() == nullptr; }));
}

INSTANTIATE_TEST_SUITE_P(All, DictationGlicBrowserTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         DictationGlicSelectionOverlayBrowserTest,
                         testing::Bool());

}  // namespace

}  // namespace dictation
