// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_controller_impl.h"

#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/state_transitions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/app/public/conversation.h"
#include "chrome/browser/ttc/core/session_journal.h"
#include "chrome/browser/ttc/core/session_view.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ttc/core/ttc_page_context_monitor.h"
#include "chrome/browser/ttc/core/voice_focused_contents_tracker.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ttc/android/session_view_android.h"
#else
#include "chrome/browser/ttc/core/session_view_impl.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#endif

namespace ttc {

namespace {

std::unique_ptr<SessionView> MakeSessionView(SessionViewDelegate& delegate) {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<SessionViewAndroid>(delegate);
#else
  return std::make_unique<SessionViewImpl>(delegate);
#endif
}

void DCheckSessionLifecycleTransition(SessionLifecycle from,
                                      SessionLifecycle to) {
#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<SessionLifecycle>>
      kTransitions(base::StateTransitions<SessionLifecycle>({
          {SessionLifecycle::kInitializing,
           {SessionLifecycle::kLive, SessionLifecycle::kFinished}},
          {SessionLifecycle::kLive, {SessionLifecycle::kFinished}},
          {SessionLifecycle::kFinished, {}},
      }));
  DCHECK_STATE_TRANSITION(kTransitions, from, to);
#endif  // DCHECK_IS_ON()
}

}  // namespace

SessionControllerImpl::SessionControllerImpl(TtcKeyedService& service)
    : service_(service),
      session_view_(MakeSessionView(*this)),
      voice_focused_contents_tracker_(
          VoiceFocusedContentsTracker::Create(CHECK_DEREF(service.profile()))),
      tool_controller_(*this) {
  GetJournal().Log("TtcSessionStart", {});

  voice_focused_contents_tracker_observation_.Observe(
      voice_focused_contents_tracker_.get());

  // Created here rather than in the initializer list because MakeConversation()
  // calls back into GetProfile() on this object.
  conversation_ =
      service.MakeConversation(base::PassKey<SessionControllerImpl>(), *this);
  conversation_->Start();
}

SessionControllerImpl::~SessionControllerImpl() {
  conversation_->Stop();

  GetJournal().Log(
      "TtcSessionEnd",
      actor::JournalDetailsBuilder().Add("state", session_lifecycle_).Build());
}

SessionJournal& SessionControllerImpl::GetJournal() {
  return tool_controller_.journal();
}

SessionLifecycle SessionControllerImpl::GetSessionLifecycle() const {
  return session_lifecycle_;
}

void SessionControllerImpl::SetSessionLifecycle(SessionLifecycle lifecycle) {
  if (session_lifecycle_ == lifecycle) {
    return;
  }

  DCheckSessionLifecycleTransition(session_lifecycle_, lifecycle);

  GetJournal().Log("TtcSessionLifecycle",
                   actor::JournalDetailsBuilder()
                       .Add("current_state", session_lifecycle_)
                       .Add("new_state", lifecycle)
                       .Build());
  session_lifecycle_ = lifecycle;
}

Profile* SessionControllerImpl::GetProfile() {
  return service_->profile();
}

void SessionControllerImpl::ProcessToolCall(
    const ToolRequest& tool_request,
    ToolResponseCallback tool_response_callback) {
  tool_controller_.ProcessToolCall(tool_request,
                                   std::move(tool_response_callback));
}

std::vector<ToolDefinition> SessionControllerImpl::GetToolDefinitions() {
  return tool_controller_.GetToolDefinitions();
}

void SessionControllerImpl::UserAudioLevelUpdate(float audio_level) {
  session_view_->UpdateAudioLevel(audio_level);
}

BrowserWindowInterface* SessionControllerImpl::GetBrowserWindowInterface() {
#if BUILDFLAG(IS_ANDROID)
  return nullptr;
#else
  ProfileBrowserCollection* browsers =
      ProfileBrowserCollection::GetForProfile(service_->profile());
  return browsers ? browsers->GetLastActiveBrowser() : nullptr;
#endif
}

void SessionControllerImpl::OnSessionInitialized() {
  session_view_->OnSessionInitialized();
}

void SessionControllerImpl::OnError(ErrorCode error) {
  const bool is_fatal = IsFatal(error);
  GetJournal().Log("TtcError", actor::JournalDetailsBuilder()
                                   .AddError(error)
                                   .Add("fatal", is_fatal)
                                   .Build());

  if (fatal_error_reported_) {
    return;
  }

  if (is_fatal) {
    EndSessionAsync();
    fatal_error_reported_ = true;
  }

  session_view_->OnError(error);
}

void SessionControllerImpl::EndSessionAsync() {
  SetSessionLifecycle(SessionLifecycle::kFinished);

  // Ending the session destroys this object so it must be done asynchronously.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&TtcKeyedService::EndSession, service_->GetWeakPtr()));
}

void SessionControllerImpl::OnVoiceFocusedContentsChanged(
    content::WebContents* web_contents) {
  if (!page_context_monitor_ && !web_contents) {
    return;
  }
  page_context_monitor_.reset();
  if (web_contents) {
    page_context_monitor_ = std::make_unique<TtcPageContextMonitor>(
        *web_contents,
        base::BindRepeating(&Conversation::OnPageContextInvalidated,
                            base::Unretained(conversation_.get())),
        base::BindRepeating(&SessionControllerImpl::OnPageContextFetched,
                            base::Unretained(this)));
  }
  conversation_->OnPageContextInvalidated();
}

content::WebContents* SessionControllerImpl::GetVoiceFocusedWebContents() {
  return voice_focused_contents_tracker_->GetActiveWebContents();
}

void SessionControllerImpl::OnPageContextFetched(
    const PageContextResult& result) {
  if (!result.has_value()) {
    // TODO(b/555804152): Signal to the model when page context cannot be
    // fetched or is ineligible.
    return;
  }
  const optimization_guide::proto::AnnotatedPageContent& apc =
      result->annotated_page_content->data;
  conversation_->SendContextUpdate(GURL(apc.main_frame_data().url()),
                                   apc.main_frame_data().title(), apc);
}

}  // namespace ttc
