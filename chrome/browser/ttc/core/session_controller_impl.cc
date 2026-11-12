// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_controller_impl.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check_deref.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/state_transitions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/app/public/audio_controller.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/lock_or_suspend_detector.h"
#include "chrome/browser/ttc/core/pointer_input_observer.h"
#include "chrome/browser/ttc/core/pointer_tracker.h"
#include "chrome/browser/ttc/core/session_inactivity_detector.h"
#include "chrome/browser/ttc/core/session_journal_impl.h"
#include "chrome/browser/ttc/core/session_view.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ttc/core/ttc_page_context_monitor.h"
#include "chrome/browser/ttc/core/voice_focused_contents_tracker.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/ttc/app/public/conversation.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ttc {

namespace {

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

PointerTracker::Config PointerTrackerConfigFromFeatureParams() {
  PointerTracker::Config config;
  config.dwell_radius_dip =
      static_cast<float>(kTtcGetPointerDwellRadiusDip.Get());
  config.min_dwell_duration = kTtcGetPointerMinDwellDuration.Get();
  config.pre_turn_lookback = kTtcGetPointerPreTurnLookback.Get();
  config.idle_limit = kTtcGetPointerIdleLimit.Get();
  // Zero or fewer results would make the tool useless, or wrap on the cast.
  config.max_results = std::max(1, kTtcGetPointerMaxResults.Get());
  return config;
}

}  // namespace

SessionControllerImpl::SessionControllerImpl(
    TtcKeyedService& service,
    std::unique_ptr<AudioController> audio_controller)
    : service_(service),
      session_view_(
          service.MakeSessionView(base::PassKey<SessionControllerImpl>(),
                                  *this)),
      voice_focused_contents_tracker_(
          VoiceFocusedContentsTracker::Create(CHECK_DEREF(service.profile()))),
      actor_task_holder_(*this),
      journal_(CHECK_DEREF(actor::ActorKeyedService::Get(service.profile()))
                   .GetJournal(),
               actor_task_holder_.task_id()),
      tool_controller_(*this) {
  // Not using AddObserver() because its immediate callback would reach Java on
  // Android before the session is stored. The view starts initializing anyway.
  observers_.AddObserver(session_view_.get());
  GetJournal().Log("TtcSessionStart", {});

  if (base::FeatureList::IsEnabled(kTtcGetPointer)) {
    pointer_tracker_ = std::make_unique<PointerTracker>(
        PointerTrackerConfigFromFeatureParams());
  }
  voice_focused_contents_tracker_observation_.Observe(
      voice_focused_contents_tracker_.get());

  // Created here rather than in the initializer list because MakeConversation()
  // calls back into GetProfile() on this object.
  conversation_ =
      service.MakeConversation(base::PassKey<SessionControllerImpl>(), *this,
                               std::move(audio_controller));
  conversation_->Start();

  inactivity_detector_ = std::make_unique<SessionInactivityDetector>(
      *this, base::BindRepeating(&SessionControllerImpl::OnInactivityTimeout,
                                 weak_ptr_factory_.GetWeakPtr()));
  lock_or_suspend_detector_ = std::make_unique<LockOrSuspendDetector>(
      base::BindRepeating(&SessionControllerImpl::OnLockedOrSuspended,
                          weak_ptr_factory_.GetWeakPtr()));
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
  // Only a live session can be muted.
  if (lifecycle != SessionLifecycle::kLive) {
    app_mic_muted_ = false;
  }
  for (SessionController::Observer& observer : observers_) {
    observer.OnSessionLifecycleChanged(lifecycle);
  }
}

void SessionControllerImpl::SetSessionId(const std::string& session_id) {
  actor_task_holder_.SetSessionId(session_id);
}

Profile* SessionControllerImpl::GetProfile() {
  return service_->profile();
}

void SessionControllerImpl::ProcessToolCall(
    const ToolRequest& tool_request,
    ToolResponseCallback tool_response_callback) {
  tool_controller_.ProcessToolCall(
      tool_request,
      // Unretained is safe: `tool_controller_` is owned by `this` and only runs
      // the callback while it's alive.
      base::BindOnce(&SessionControllerImpl::NotifyToolExecutionCompleted,
                     base::Unretained(this), tool_request.Clone())
          .Then(std::move(tool_response_callback)));
}

void SessionControllerImpl::ProcessToolCallBatch(
    std::vector<PendingToolCall> tool_calls) {
  for (PendingToolCall& tool_call : tool_calls) {
    // Unretained is safe: `tool_controller_` is owned by `this` and only runs
    // the callback while it's alive.
    tool_call.callback =
        base::BindOnce(&SessionControllerImpl::NotifyToolExecutionCompleted,
                       base::Unretained(this), tool_call.request.Clone())
            .Then(std::move(tool_call.callback));
  }
  tool_controller_.ProcessToolCallBatch(std::move(tool_calls));
}

std::vector<ToolDefinition> SessionControllerImpl::GetToolDefinitions() {
  return tool_controller_.GetToolDefinitions();
}

void SessionControllerImpl::UserAudioLevelUpdate(float audio_level) {
  for (SessionController::Observer& observer : observers_) {
    observer.OnUserAudioLevelUpdate(audio_level);
  }
}

void SessionControllerImpl::OnMicCaptureChanged(bool capturing) {
  if (session_lifecycle_ == SessionLifecycle::kFinished) {
    return;
  }
  for (SessionController::Observer& observer : observers_) {
    observer.OnMicCaptureChanged(capturing);
  }
}

void SessionControllerImpl::OnSystemMicMuteChanged(bool muted,
                                                   bool at_stream_start) {
  GetJournal().Log("TtcSystemMicMuted",
                   actor::JournalDetailsBuilder()
                       .Add("muted", muted)
                       .Add("at_stream_start", at_stream_start)
                       .Build());
  if (session_lifecycle_ == SessionLifecycle::kFinished) {
    return;
  }
  for (SessionController::Observer& observer : observers_) {
    observer.OnSystemMicMuteChanged(muted, at_stream_start);
  }
}

BrowserWindowInterface* SessionControllerImpl::GetBrowserWindowInterface() {
  return voice_focused_contents_tracker_->GetActiveBrowserWindow();
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
  transcript_tracker_.OnModelTurnComplete();
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
  transcript_tracker_.AddUserTranscription(text);
  NotifyTranscriptsUpdated();
}

void SessionControllerImpl::OnOutputTranscription(const std::string& text) {
  for (SessionController::Observer& observer : observers_) {
    observer.OnOutputTranscription(text);
  }
  transcript_tracker_.AddModelTranscription(text);
  NotifyTranscriptsUpdated();
}

void SessionControllerImpl::OnModelAudioEnvelopeReady(
    const AudioEnvelope& envelope) {
  for (SessionController::Observer& observer : observers_) {
    observer.OnModelAudioEnvelopeReady(envelope);
  }
}

ToolResponse SessionControllerImpl::NotifyToolExecutionCompleted(
    const ToolRequest& tool_request,
    ToolResponse response) {
  for (SessionController::Observer& observer : observers_) {
    observer.OnToolExecutionCompleted(tool_request.name, tool_request.arguments,
                                      response.Ok());
  }
  return response;
}

void SessionControllerImpl::NotifyTranscriptsUpdated() {
  // A finished session no longer reports transcripts.
  if (session_lifecycle_ == SessionLifecycle::kFinished) {
    return;
  }
  for (SessionController::Observer& observer : observers_) {
    observer.OnTranscriptionsUpdated(transcript_tracker_.user_transcript(),
                                     transcript_tracker_.model_transcript());
  }
}

void SessionControllerImpl::OnSpeakerChanged(Speaker speaker) {
  // A finished session no longer reports who's speaking.
  if (speaker_ == speaker ||
      session_lifecycle_ == SessionLifecycle::kFinished) {
    return;
  }
  speaker_ = speaker;
  if (speaker_ == Speaker::kUser) {
    user_turn_start_ = base::TimeTicks::Now();
  }
  for (SessionController::Observer& observer : observers_) {
    observer.OnSpeakerChanged(speaker_);
  }
}

Speaker SessionControllerImpl::GetSpeaker() const {
  return speaker_;
}

bool SessionControllerImpl::SetMicMuted(bool muted) {
  // Only a live session can be muted.
  if (session_lifecycle_ != SessionLifecycle::kLive ||
      app_mic_muted_ == muted) {
    return false;
  }
  GetJournal().Log("TtcMicMuted",
                   actor::JournalDetailsBuilder().Add("muted", muted).Build());
  app_mic_muted_ = muted;
  conversation_->SetMicMuted(muted);
  for (SessionController::Observer& observer : observers_) {
    observer.OnMicMutedChanged(muted);
  }
  return true;
}

bool SessionControllerImpl::IsMicMuted() const {
  return app_mic_muted_;
}

void SessionControllerImpl::EndSessionAsync() {
  SetSessionLifecycle(SessionLifecycle::kFinished);

  // Ending the session destroys this object so it must be done asynchronously.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&TtcKeyedService::EndSession, service_->GetWeakPtr()));
}

void SessionControllerImpl::SendTextInput(const std::string& text) {
  user_turn_start_ = base::TimeTicks::Now();
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
        *web_contents, GetJournal(), service_->enterprise_policy_checker(),
        base::BindRepeating(&Conversation::OnPageContextInvalidated,
                            base::Unretained(conversation_.get())),
        base::BindRepeating(&SessionControllerImpl::OnPageContextFetched,
                            base::Unretained(this)));
  }
  if (pointer_tracker_) {
    // Positions were relative to the old contents.
    pointer_tracker_->Clear();
    pointer_input_observer_ = web_contents
                                  ? std::make_unique<PointerInputObserver>(
                                        pointer_tracker_.get(), web_contents)
                                  : nullptr;
  }
  conversation_->OnPageContextInvalidated();
}

content::WebContents* SessionControllerImpl::GetVoiceFocusedWebContents() {
  return voice_focused_contents_tracker_->GetActiveWebContents();
}

actor::TaskId SessionControllerImpl::GetActorTaskId() const {
  return actor_task_holder_.task_id();
}

void SessionControllerImpl::EnsureActorTaskCreated() {
  // TODO(b/552544497): Ideally the task would stop only if the Ttc session
  // ends but there are currently a few ways for tasks to be stopped
  // externally.
  actor_task_holder_.EnsureTaskCreated();
}

void SessionControllerImpl::OnPageContextFetched(
    const PageContextResult& result) {
  if (!result.has_value()) {
    // Replace the model's context with just the tab, so that it doesn't keep
    // using context of a page that's no longer shown. Only share the origin of
    // a page whose context may not be shared.
    // TODO(b/555804152): Tell the model why there's no page content.
    content::WebContents* contents = GetVoiceFocusedWebContents();
    if (!contents) {
      return;
    }
    content::RenderFrameHost* main_frame = contents->GetPrimaryMainFrame();
    if (result.error() == page_content_annotations::FetchPageContextError::
                              kPageContextNotEligible) {
      conversation_->SendContextUpdate(
          main_frame->GetLastCommittedOrigin().GetURL(),
          /*title=*/std::string(), /*apc=*/{});
    } else {
      conversation_->SendContextUpdate(main_frame->GetLastCommittedURL(),
                                       base::UTF16ToUTF8(contents->GetTitle()),
                                       /*apc=*/{});
    }
    return;
  }
  const optimization_guide::proto::AnnotatedPageContent& apc =
      result->annotated_page_content->data;
  conversation_->SendContextUpdate(GURL(apc.main_frame_data().url()),
                                   apc.main_frame_data().title(), apc);
}

void SessionControllerImpl::OnInactivityTimeout() {
  if (session_lifecycle_ == SessionLifecycle::kFinished) {
    return;
  }
  GetJournal().Log(
      "TtcSessionTimeout",
      actor::JournalDetailsBuilder()
          .Add("reason", "inactivity")
          .Add("timeout_seconds", kSessionInactivityTimeout.Get().InSeconds())
          .Build());

  OnError(ErrorCode::kInactivityTimeout);
}

void SessionControllerImpl::OnLockedOrSuspended(
    LockOrSuspendDetector::Reason reason) {
  if (session_lifecycle_ == SessionLifecycle::kFinished) {
    return;
  }
  // The user locked the screen or suspended the device, so there is no error
  // to report.
  GetJournal().Log(
      "TtcSessionLockedOrSuspended",
      actor::JournalDetailsBuilder()
          .Add("reason", reason == LockOrSuspendDetector::Reason::kSuspend
                             ? "suspend"
                             : "screen_lock")
          .Build());
  EndSessionAsync();
}

}  // namespace ttc
