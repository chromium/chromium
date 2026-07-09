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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/dictation/onboarding_dialog_controller.h"
#include "chrome/common/extensions/api/dictation_private.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
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

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
                       ShouldShowContextMenuItem) {
  EXPECT_TRUE(dictation_service().ShouldShowContextMenuItem());

  StartSession();

  EXPECT_FALSE(dictation_service().ShouldShowContextMenuItem());

  dictation_service().EndSession();

  EXPECT_TRUE(dictation_service().ShouldShowContextMenuItem());
}

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
                       ExecuteContextMenuCommand) {
  content::ContextMenuParams params;
  params.is_editable = true;
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
  controller->StartDictationStream(DefaultInPageTargetId(web_contents()));
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

  StartSession();

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

  // Start a new session and stream, commit some text, and stop.
  {
    StartSession();

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

}  // namespace

}  // namespace dictation
