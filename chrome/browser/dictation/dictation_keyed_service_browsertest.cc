// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_keyed_service.h"

#include "base/memory/weak_ptr.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/dictation/dictation_browser_test_base.h"
#include "chrome/browser/dictation/dictation_keyed_service_factory.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/dictation/stream_provider.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/dictation/onboarding_dialog_controller.h"
#include "chrome/common/extensions/api/dictation_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/browser/global_dom_node_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/blink/public/common/dom/dom_node_id.h"
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

class FocusLossObserver : public content::WebContentsObserver {
 public:
  explicit FocusLossObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  FocusLossObserver(const FocusLossObserver&) = delete;
  FocusLossObserver& operator=(const FocusLossObserver&) = delete;
  ~FocusLossObserver() override = default;

  // content::WebContentsObserver:
  void OnWebContentsLostFocus(
      content::RenderWidgetHost* render_widget_host) override {
    lost_focus_called_ = true;
  }

  bool lost_focus_called() const { return lost_focus_called_; }

 private:
  bool lost_focus_called_ = false;
};

class DictationKeyedServiceBrowserTest : public DictationBrowserTestBase {
 public:
  DictationKeyedServiceBrowserTest() = default;
  ~DictationKeyedServiceBrowserTest() override = default;

  void SimulateSpeechRecognition(ListenerStreamProvider* provider,
                                 ExtensionTranscriptionType type,
                                 std::string_view text) {
    ExtensionSendTranscriptUpdate(profile(), provider->stream_id_for_testing(),
                                  type, text);
  }

  // There's no great way to wait on the dictation target to have fully
  // committed text (i.e. visible to the page) so this method will poll until
  // the editable shows the expected text. Return the string for ergonomics so
  // that a failure can show up as a failing EXPECT_EQ.
  std::string GetEditableExpectedText(std::string_view selector,
                                      std::string_view expected) {
    std::string last_seen_string;
    EXPECT_TRUE(base::test::RunUntil([&]() {
      last_seen_string =
          content::EvalJs(
              web_contents(),
              content::JsReplace("document.querySelector($1).value;", selector))
              .ExtractString();
      return last_seen_string == expected;
    }));

    return last_seen_string;
  }
};

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
                       CreatedForRegularProfile) {
  EXPECT_NE(DictationKeyedService::Get(profile()), nullptr);
}

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
                       NotCreatedForOTRProfile) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_EQ(DictationKeyedService::Get(otr_profile), nullptr);
}

class DictationKeyedServiceDisabledBrowserTest
    : public DictationKeyedServiceBrowserTest {
 public:
  DictationKeyedServiceDisabledBrowserTest() {
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
IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
                       ShouldShowContextMenuItem) {
  EXPECT_TRUE(dictation_service().ShouldShowContextMenuItem());

  StartSession();

  EXPECT_TRUE(dictation_service().ShouldShowContextMenuItem());

  dictation_service().EndSession();

  EXPECT_TRUE(dictation_service().ShouldShowContextMenuItem());
}

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
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
IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
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
IN_PROC_BROWSER_TEST_F(
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
  Browser* second_browser = CreateBrowser(profile());
  content::WebContents* window2_contents =
      second_browser->tab_strip_model()->GetActiveWebContents();
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

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
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

// TODO(crbug.com/502587072): Add tests which have the test extension simulate
// stream failures, including on start and mid stream.

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
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
  provider->Stop();
  ExtensionSendStreamStateUpdate(profile(), provider->stream_id_for_testing(),
                                 ExtensionStreamState::kComplete);
  EXPECT_EQ(controller->GetState(), SessionState::kInactive);
}

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
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

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
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
                                   DictationStreamStartTrigger::kSessionStart);
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

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
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
  provider->Stop();
  ExtensionSendStreamStateUpdate(profile(), provider->stream_id_for_testing(),
                                 ExtensionStreamState::kComplete);
  EXPECT_TRUE(base::test::RunUntil([&]() { return provider_weak == nullptr; }));
}

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
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

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
                       TranscriptionCommittedToElement) {
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(web_contents());
  content::MainThreadFrameObserver frame_observer(
      web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
  frame_observer.Wait();

  FocusLossObserver focus_loss_observer(web_contents());

  // Focus the textarea so that dictation targets it.
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "text_id");

  if (!content::IsRenderWidgetHostFocused(
          web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost())) {
    GTEST_SKIP() << "Test is sensitive to focus loss from test environment "
                    "until crbug.com/525856380 is fixed.";
  }

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

  provider->Stop();
  ExtensionSendStreamStateUpdate(profile(), provider->stream_id_for_testing(),
                                 ExtensionStreamState::kComplete);

  if (focus_loss_observer.lost_focus_called()) {
    GTEST_SKIP() << "Test is sensitive to focus loss from test environment "
                    "until crbug.com/525856380 is fixed.";
  }

  // Verify the transcription reached the document.
  EXPECT_EDITABLE_TEXT_EQ("#text_id", "Hello World");
}

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
                       ToggleStreamAndCommit) {
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(web_contents());
  content::MainThreadFrameObserver frame_observer(
      web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
  frame_observer.Wait();

  FocusLossObserver focus_loss_observer(web_contents());

  // Focus the textarea so that dictation targets it.
  content::SimulateMouseClickOrTapElementWithId(web_contents(), "text_id");

  if (!content::IsRenderWidgetHostFocused(
          web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost())) {
    GTEST_SKIP() << "Test is sensitive to focus loss from test environment "
                    "until crbug.com/525856380 is fixed.";
  }

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

  if (focus_loss_observer.lost_focus_called()) {
    GTEST_SKIP() << "Test is sensitive to focus loss from test environment "
                    "until crbug.com/525856380 is fixed.";
  }

  EXPECT_EDITABLE_TEXT_EQ("#text_id", "Hello World");
}

// TODO(b/533465625): Ideally we could also make this a child of
// DictationBrowserTestBase so we get all the helpers.
// TODO(crbug.com/537848278): Simplify this test suite to GlicBrowserTest.
class DictationGlicBrowserTest : public glic::NonInteractiveGlicTest {
 public:
  DictationGlicBrowserTest()
      : scoped_feature_list_(CreateEnablingFeatureList()) {}
  ~DictationGlicBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    glic::NonInteractiveGlicTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
        std::string(kDictationTestExtensionId));
  }

  void SetUpOnMainThread() override {
    glic::NonInteractiveGlicTest::SetUpOnMainThread();
    GetProfile()->GetPrefs()->SetBoolean(
        prefs::kPrefDictationOnboardingCompleted, true);
    LoadTestExtensionInManualMode(GetProfile());
  }

  DictationKeyedService& dictation_service() {
    return *DictationKeyedService::Get(GetProfile());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensure basic stream setup, state changes, and end work correctly for streams
// started for a Glic guest.
IN_PROC_BROWSER_TEST_F(DictationGlicBrowserTest, BasicStreamFunctions) {
  RunTestSequence(OpenGlic(), CheckGlicInstanceIsShowing());

  content::RenderFrameHost* glic_rfh = FindGlicGuestMainFrame();
  ASSERT_TRUE(glic_rfh);

  // Start a session using the Glic guest document rather than the normal tab
  // document.
  content::GlobalDOMNodeId target_id(glic_rfh->GetWeakDocumentPtr(),
                                     blink::DOMNodeIdType(123));

  tabs::TabInterface* tab = chrome_test_utils::GetActiveTab(this);
  CHECK(tab);
  dictation_service().StartSession(*tab, TargetDetails(target_id),
                                   DictationSessionEntryPoint::kContextMenu);

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
  EXPECT_EQ(controller->GetState(), SessionState::kInactive);

  dictation_service().EndSession();
  RunTestSequence(CloseGlic());
}

}  // namespace

}  // namespace dictation
