// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_interactive_browser_test_base.h"

#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/dictation/session_controller.h"
#include "chrome/browser/dictation/session_ui_impl.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

namespace dictation {

DictationInteractiveBrowserTestBase::DictationInteractiveBrowserTestBase() =
    default;

DictationInteractiveBrowserTestBase::~DictationInteractiveBrowserTestBase() =
    default;

void DictationInteractiveBrowserTestBase::TearDownOnMainThread() {
  dictation_service().EndSession();
  InteractiveBrowserTestMixin<DictationBrowserTestBase>::TearDownOnMainThread();
}

SessionUiImpl* DictationInteractiveBrowserTestBase::session_ui() {
  if (!dictation_service().session_controller()) {
    return nullptr;
  }
  return static_cast<SessionUiImpl*>(
      dictation_service().session_controller()->ui_for_testing());
}

content::WebContents* DictationInteractiveBrowserTestBase::web_contents() {
  return chrome_test_utils::GetActiveWebContents(this);
}

DictationInteractiveBrowserTestBase::StepBuilder
DictationInteractiveBrowserTestBase::CheckHasSession(
    bool expected_has_session) {
  return CheckResult(
      [this]() { return dictation_service().session_controller() != nullptr; },
      expected_has_session);
}

DictationInteractiveBrowserTestBase::MultiStep
DictationInteractiveBrowserTestBase::StartSession(
    std::unique_ptr<TargetDetails> target_details) {
  return Steps(
      Do([this, target_details = std::move(target_details)] {
        TargetDetails target = target_details
                                   ? *target_details
                                   : DefaultInPageTarget(web_contents());
        content::RenderFrameHost* rfh =
            target.target_id.document.AsRenderFrameHostIfValid();
        CHECK(rfh);
        content::WebContents* web_contents =
            content::WebContents::FromRenderFrameHost(rfh);
        CHECK(web_contents);
        tabs::TabInterface* tab =
            tabs::TabInterface::GetFromContents(web_contents);
        CHECK(tab);
        dictation_service().StartSessionForTesting(
            *tab, target, DictationSessionEntryPoint::kContextMenu);
        if (dictation_service().session_controller()) {
          last_started_provider_ = static_cast<ListenerStreamProvider*>(
                                       dictation_service()
                                           .session_controller()
                                           ->attached_stream_provider())
                                       ->GetWeakPtr();

          // Register callback to update last_started_provider_ on subsequent
          // starts.
          session_state_subscription_ =
              dictation_service()
                  .session_controller()
                  ->AddSessionStateChangedCallback(
                      base::BindRepeating(&DictationInteractiveBrowserTestBase::
                                              OnSessionStateChanged,
                                          base::Unretained(this)));
        }
      }),
      ExtensionAPIWaitForStreamStart());
}

DictationInteractiveBrowserTestBase::MultiStep
DictationInteractiveBrowserTestBase::StartSession() {
  return StartSession(nullptr);
}

DictationInteractiveBrowserTestBase::MultiStep
DictationInteractiveBrowserTestBase::StartSessionWithTarget(
    ui::ElementIdentifier web_contents_id,
    std::string_view query_selector) {
  const DeepQuery where{std::string(query_selector)};
  auto target_details = std::make_unique<TargetDetails>();
  TargetDetails* raw_target_details = target_details.get();
  return Steps(
      ExecuteJsAt(web_contents_id, where, "el => el.focus()"),
      WithElement(
          web_contents_id,
          [raw_target_details,
           selector_str = std::string(query_selector)](ui::TrackedElement* el) {
            content::WebContents* wc =
                AsInstrumentedWebContents(el)->web_contents();
            CHECK(wc);
            // TODO(mcnee): Don't assume the element is in the main frame.
            std::optional<int> dom_node_id =
                content::GetDOMNodeId(*wc->GetPrimaryMainFrame(), selector_str);
            CHECK(dom_node_id.has_value());

            // TODO(mcnee): Set whether the target is richly editable.
            *raw_target_details = TargetDetails(content::GlobalDOMNodeId{
                wc->GetPrimaryMainFrame()->GetWeakDocumentPtr(),
                blink::DOMNodeIdType(dom_node_id.value())});
          }),
      StartSession(std::move(target_details)));
}

DictationInteractiveBrowserTestBase::MultiStep
DictationInteractiveBrowserTestBase::ExtensionAPISetStreamState(
    ExtensionStreamState state) {
  return Steps(Do([this, state] {
    ASSERT_NE(last_started_provider_, nullptr);
    ExtensionSendStreamStateUpdate(
        profile(), last_started_provider_->stream_id_for_testing(), state);
  }));
}

DictationInteractiveBrowserTestBase::MultiStep
DictationInteractiveBrowserTestBase::ExtensionAPISetStreamState(
    const StreamId& stream_id,
    ExtensionStreamState state) {
  return Steps(Do([this, &stream_id, state] {
    ExtensionSendStreamStateUpdate(profile(), stream_id, state);
  }));
}

DictationInteractiveBrowserTestBase::MultiStep
DictationInteractiveBrowserTestBase::ExtensionAPIUpdateTranscription(
    ExtensionTranscriptionType type,
    std::string_view text) {
  return Steps(Do([this, type, text_str = std::string(text)] {
    ASSERT_NE(last_started_provider_, nullptr);
    ExtensionSendTranscriptUpdate(
        profile(), last_started_provider_->stream_id_for_testing(), type,
        text_str);
  }));
}

DictationInteractiveBrowserTestBase::MultiStep
DictationInteractiveBrowserTestBase::ExtensionAPIUpdateTranscription(
    const StreamId& stream_id,
    ExtensionTranscriptionType type,
    std::string_view text) {
  return Steps(Do([this, &stream_id, type, text_str = std::string(text)] {
    ExtensionSendTranscriptUpdate(profile(), stream_id, type, text_str);
  }));
}

DictationInteractiveBrowserTestBase::MultiStep
DictationInteractiveBrowserTestBase::ExtensionAPIWaitForStreamStart() {
  return Steps(Do([this] {
    // A stream may not always be created (e.g. onboarding needs to be shown).
    if (last_started_provider_) {
      ExtensionWaitForStreamStart(
          profile(), last_started_provider_->stream_id_for_testing());
    }
  }));
}

base::RepeatingCallback<SessionState()>
DictationInteractiveBrowserTestBase::GetSessionState() {
  return base::BindRepeating(
      [](DictationInteractiveBrowserTestBase* test) {
        return test->dictation_service().session_controller()->GetState();
      },
      base::Unretained(this));
}

base::RepeatingCallback<bool()>
DictationInteractiveBrowserTestBase::HasAttachedStreamProvider() {
  return base::BindRepeating(
      [](DictationInteractiveBrowserTestBase* test) {
        return test->dictation_service()
                   .session_controller()
                   ->attached_stream_provider() != nullptr;
      },
      base::Unretained(this));
}

void DictationInteractiveBrowserTestBase::OnSessionStateChanged(
    SessionState state) {
  if (state == SessionState::kStreamInitializing) {
    auto* controller = dictation_service().session_controller();
    if (controller && controller->attached_stream_provider()) {
      last_started_provider_ = static_cast<ListenerStreamProvider*>(
                                   controller->attached_stream_provider())
                                   ->GetWeakPtr();
    }
  }
}

}  // namespace dictation
