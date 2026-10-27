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
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/app/public/audio_controller.h"
#include "chrome/browser/ttc/app/public/conversation.h"
#include "chrome/browser/ttc/core/session_journal_impl.h"
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

SessionControllerImpl::SessionControllerImpl(
    TtcKeyedService& service,
    std::unique_ptr<AudioController> audio_controller)
    : service_(service),
      actor_task_holder_(*this),
      journal_(CHECK_DEREF(actor::ActorKeyedService::Get(service.profile()))
                   .GetJournal(),
               actor_task_holder_.task_id()),
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
      service.MakeConversation(base::PassKey<SessionControllerImpl>(), *this,
                               std::move(audio_controller));
  conversation_->Start();
}

SessionControllerImpl::~SessionControllerImpl() {
  if (session_lifecycle_ != SessionLifecycle::kFinished) {
    for (SessionController::Observer& observer : observers_) {
      observer.OnSessionLifecycleChanged(SessionLifecycle::kFinished);
    }
  }
  observers_.Clear();

  conversation_->Stop();

  GetJournal().Log(
      "TtcSessionEnd",
      actor::JournalDetailsBuilder().Add("state", session_lifecycle_).Build());
}

SessionJournalImpl& SessionControllerImpl::GetJournal() {
  return journal_;
}

DebugSink* SessionControllerImpl::GetDebugSink() {
  // Nothing records sessions yet.
  return nullptr;
}

void SessionControllerImpl::AddObserver(SessionController::Observer* observer) {
  observers_.AddObserver(observer);
  observer->OnSessionLifecycleChanged(session_lifecycle_);
}

void SessionControllerImpl::RemoveObserver(
    SessionController::Observer* observer) {
  observers_.RemoveObserver(observer);
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
  for (SessionController::Observer& observer : observers_) {
    observer.OnSessionLifecycleChanged(lifecycle);
  }
}

Profile* SessionControllerImpl::GetProfile() {
  return service_->profile();
}

void SessionControllerImpl::ProcessToolCall(
    const ToolRequest& tool_request,
    ToolResponseCallback tool_response_callback) {
  // TODO(b/552544497): Ideally the task would stop only if the Ttc session
  // ends but there are currently a few ways for tasks to be stopped
  // externally.
  actor_task_holder_.EnsureTaskCreated();
  std::string tool_name = tool_request.name;
  tool_controller_.ProcessToolCall(
      tool_request,
      base::BindOnce(
          [](base::WeakPtr<SessionControllerImpl> self, std::string name,
             ToolResponseCallback cb, ToolResponse response) {
            if (self) {
              for (SessionController::Observer& observer : self->observers_) {
                observer.OnToolExecutionCompleted(name, response.Ok());
              }
            }
            std::move(cb).Run(std::move(response));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(tool_name),
          std::move(tool_response_callback)));
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

  if (session_lifecycle_ == SessionLifecycle::kFinished) {
    return;
  }

  session_view_->OnError(error);
  for (SessionController::Observer& observer : observers_) {
    observer.OnError(error);
  }

  if (is_fatal) {
    EndSessionAsync();
  }
}

void SessionControllerImpl::OnModelTurnStarted() {
  for (SessionController::Observer& observer : observers_) {
    observer.OnModelTurnStarted();
  }
}

void SessionControllerImpl::OnModelTurnComplete() {
  for (SessionController::Observer& observer : observers_) {
    observer.OnModelTurnComplete();
  }
}

void SessionControllerImpl::OnModelInterrupted() {
  for (SessionController::Observer& observer : observers_) {
    observer.OnModelInterrupted();
  }
}

void SessionControllerImpl::OnInputTranscription(const std::string& text) {
  for (SessionController::Observer& observer : observers_) {
    observer.OnInputTranscription(text);
  }
}

void SessionControllerImpl::OnOutputTranscription(const std::string& text) {
  for (SessionController::Observer& observer : observers_) {
    observer.OnOutputTranscription(text);
  }
}

void SessionControllerImpl::OnModelAudioEnvelopeReady(
    const AudioEnvelope& envelope) {
  for (SessionController::Observer& observer : observers_) {
    observer.OnModelAudioEnvelopeReady(envelope);
  }
}

bool SessionControllerImpl::SetMicMuted(bool muted) {
  // Capture only starts once the session is live.
  if (session_lifecycle_ != SessionLifecycle::kLive ||
      conversation_->IsMicMuted() == muted) {
    return false;
  }
  GetJournal().Log("TtcMicMuted",
                   actor::JournalDetailsBuilder().Add("muted", muted).Build());
  conversation_->SetMicMuted(muted);
  return true;
}

bool SessionControllerImpl::IsMicMuted() const {
  return session_lifecycle_ == SessionLifecycle::kLive &&
         conversation_->IsMicMuted();
}
void SessionControllerImpl::EndSessionAsync() {
  SetSessionLifecycle(SessionLifecycle::kFinished);

  // Ending the session destroys this object so it must be done asynchronously.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&TtcKeyedService::EndSession, service_->GetWeakPtr()));
}

void SessionControllerImpl::SendTextInput(const std::string& text) {
  conversation_->SendTextInput(text);
}

void SessionControllerImpl::OnVoiceFocusedContentsChanged(
    content::WebContents* web_contents) {
  if (!page_context_monitor_ && !web_contents) {
    return;
  }
  page_context_monitor_.reset();
  if (web_contents) {
    page_context_monitor_ = std::make_unique<TtcPageContextMonitor>(
        *web_contents, GetJournal(),
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

actor::TaskId SessionControllerImpl::GetActorTaskId() const {
  return actor_task_holder_.task_id();
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
