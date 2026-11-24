// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_controller_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/check_deref.h"
#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/state_transitions.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/app/public/audio_controller.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/output_volume_monitor.h"
#include "chrome/browser/ttc/core/pointer_input_observer.h"
#include "chrome/browser/ttc/core/pointer_tracker.h"
#include "chrome/browser/ttc/core/session_end_condition_detector.h"
#include "chrome/browser/ttc/core/session_inactivity_detector.h"
#include "chrome/browser/ttc/core/session_journal_impl.h"
#include "chrome/browser/ttc/core/session_view.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ttc/core/ttc_page_context_monitor.h"
#include "chrome/browser/ttc/core/voice_focused_contents_tracker.h"
#include "chrome/services/speech/buildflags/buildflags.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/ttc/app/public/conversation.h"
#include "components/ttc/app/public/error_codes.h"
#include "components/ttc/app/public/page_context_types.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_SPEECH_SERVICE)
#include "chrome/browser/ttc/core/hotl/soda_hotl_installer.h"
#endif

#if BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)
#include "chrome/browser/ttc/debug/ttc_debug_session.h"
#include "chrome/browser/ttc/debug/ttc_debug_tools_service.h"
#include "chrome/browser/ttc/debug/ttc_tool_call_tracer.h"
#endif  // BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)

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

std::string_view SessionEndReasonToString(
    SessionEndConditionDetector::Reason reason) {
  switch (reason) {
    case SessionEndConditionDetector::Reason::kSuspend:
      return "suspend";
    case SessionEndConditionDetector::Reason::kScreenLock:
      return "screen_lock";
#if BUILDFLAG(IS_CHROMEOS)
    case SessionEndConditionDetector::Reason::kUserSwitch:
      return "user_switch";
#endif  // BUILDFLAG(IS_CHROMEOS)
  }
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
#if BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)
  if (TtcDebugToolsService* debug_tools =
          TtcDebugToolsService::Get(service.profile())) {
    debug_session_ = debug_tools->BeginSession(&journal_);
    // Unretained is safe: `tool_call_tracer_` only asks for the task while it
    // exists.
    tool_call_tracer_ = std::make_unique<TtcToolCallTracer>(
        *debug_session_,
        CHECK_DEREF(actor::ActorKeyedService::Get(service.profile()))
            .GetJournal(),
        base::BindRepeating(&SessionControllerImpl::GetActorTaskId,
                            base::Unretained(this)));
  }
#endif  // BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)

  // Not using AddObserver() because its immediate callback would reach Java on
  // Android before the session is stored. The view starts initializing anyway.
  observers_.AddObserver(session_view_.get());
  GetJournal().Log("TtcSessionStart", {});

  if (base::FeatureList::IsEnabled(kTtcGetPointer)) {
    pointer_tracker_ = std::make_unique<PointerTracker>(
        PointerTrackerConfigFromFeatureParams());
  }
#if BUILDFLAG(ENABLE_SPEECH_SERVICE)
  hotl::PrecacheSodaForHotl(CHECK_DEREF(service.profile()));
#endif
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
  session_end_condition_detector_ =
      std::make_unique<SessionEndConditionDetector>(base::BindRepeating(
          &SessionControllerImpl::OnSessionEndConditionDetected,
          weak_ptr_factory_.GetWeakPtr()));

  if (base::FeatureList::IsEnabled(kTtcSpeakerVolumeWarning)) {
    // Unretained is safe: `this` owns the monitor, which only runs the
    // callback while it's alive.
    output_volume_monitor_ = OutputVolumeMonitor::Create(base::BindRepeating(
        &SessionControllerImpl::OnOutputVolumeChanged, base::Unretained(this)));
    if (output_volume_monitor_) {
      if (std::optional<OutputVolumeState> state =
              output_volume_monitor_->GetState()) {
        OnOutputVolumeChanged(*state);
      }
    }
  }
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

#if BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)
  // Ended while `journal_`, which the record logs its open turn to, still
  // exists. The record ignores whatever `conversation_` still reports while
  // it's destroyed.
  if (debug_session_) {
    CHECK_DEREF(TtcDebugToolsService::Get(GetProfile()))
        .EndSession(debug_session_);
  }
#endif  // BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)
}

SessionJournalImpl& SessionControllerImpl::GetJournal() {
  return journal_;
}

DebugSink* SessionControllerImpl::GetDebugSink() {
#if BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)
  return debug_session_;
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)
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
#if BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)
  if (debug_session_) {
    debug_session_->OnError(base::ToString(error));
  }
#endif  // BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)

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

void SessionControllerImpl::NotifyPointerLocationsReported(
    base::span<const gfx::Point> points) {
  for (SessionController::Observer& observer : observers_) {
    observer.OnPointerLocationsReported(points);
  }
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

bool SessionControllerImpl::CanSaveDebugTrace() const {
  return SessionJournalImpl::IsInMemoryJournalEnabled();
}

void SessionControllerImpl::SaveDebugTrace() {
  service_->SaveDebugTrace(base::PassKey<SessionControllerImpl>(),
                           GetVoiceFocusedWebContents(),
                           GetBrowserWindowInterface(), journal_);
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
        *web_contents, *this, service_->enterprise_policy_checker(),
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
    content::Page& page,
    const PageContextResult& result) {
  // `page` is guaranteed to be the primary page of
  // `GetVoiceFocusedWebContents()` because:
  //
  //  a) `page_context_monitor_` is guaranteed to be monitoring
  //     `GetVoiceFocusedWebContents()`
  //  b) `page_context_monitor_` will only run this callback with the primary
  //     page of its observed WebContents
  content::WebContents* contents = GetVoiceFocusedWebContents();
  CHECK(contents);
  CHECK_EQ(&contents->GetPrimaryPage(), &page);

  if (!result.has_value()) {
#if BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)
    if (debug_session_) {
      debug_session_->OnPageContextFetchFailed(contents->GetLastCommittedURL(),
                                               result.error());
    }
#endif  // BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)
    // Replace the model's context with just the tab, so that it doesn't keep
    // using context of a page that's no longer shown, or whose content isn't
    // available (e.g. a PDF). Only share the origin of a page whose context
    // may not be shared.
    // TODO(b/555804152): Tell the model why there's no page content.
    content::RenderFrameHost& main_frame = page.GetMainDocument();
    const TabContextInfo tab =
        result.error() == page_content_annotations::FetchPageContextError::
                              kPageContextNotEligible
            ? TabContextInfo(main_frame.GetLastCommittedOrigin().GetURL(),
                             /*title=*/std::string(),
                             /*mime_type=*/std::string())
            : TabContextInfo(main_frame.GetLastCommittedURL(),
                             base::UTF16ToUTF8(contents->GetTitle()),
                             page.GetContentsMimeType());
    conversation_->SendContextUpdate(SharedPageContext(tab));
    return;
  }
  const optimization_guide::proto::AnnotatedPageContent& apc =
      result->annotated_page_content->data;
  conversation_->SendContextUpdate(SharedPageContext(
      TabContextInfo(GURL(apc.main_frame_data().url()),
                     apc.main_frame_data().title(), page.GetContentsMimeType()),
      apc));
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

void SessionControllerImpl::OnSessionEndConditionDetected(
    SessionEndConditionDetector::Reason reason) {
  if (session_lifecycle_ == SessionLifecycle::kFinished) {
    return;
  }
  // The user locked the screen, suspended the device, or switched users, so
  // there is no error to report.
  GetJournal().Log("TtcSessionEndConditionDetected",
                   actor::JournalDetailsBuilder()
                       .Add("reason", SessionEndReasonToString(reason))
                       .Build());
  EndSessionAsync();
}

void SessionControllerImpl::OnOutputVolumeChanged(
    const OutputVolumeState& state) {
  actor::JournalDetailsBuilder details;
  details.Add("muted", state.muted);
  if (state.level) {
    details.Add("level", *state.level);
  }
  GetJournal().Log("TtcSystemSpeakerVolume", std::move(details).Build());

  output_volume_ = state;
}

}  // namespace ttc
