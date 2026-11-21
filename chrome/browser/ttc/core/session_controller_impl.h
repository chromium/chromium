// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_SESSION_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_TTC_CORE_SESSION_CONTROLLER_IMPL_H_

#include <memory>
#include <string>

#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ttc/app/public/audio_controller.h"
#include "chrome/browser/ttc/app/public/audio_envelope.h"
#include "chrome/browser/ttc/core/actor_task_holder.h"
#include "chrome/browser/ttc/core/buildflags.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/session_end_condition_detector.h"
#include "chrome/browser/ttc/core/session_journal_impl.h"
#include "chrome/browser/ttc/core/session_view_delegate.h"
#include "chrome/browser/ttc/core/states.h"
#include "chrome/browser/ttc/core/tool_controller.h"
#include "chrome/browser/ttc/core/transcript_tracker.h"
#include "chrome/browser/ttc/core/ttc_page_context_monitor.h"
#include "chrome/browser/ttc/core/voice_focused_contents_tracker.h"
#include "components/actor/core/task_id.h"
#include "components/ttc/app/public/conversation.h"

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace gfx {
class Point;
}  // namespace gfx

namespace ttc {

class PointerInputObserver;
class PointerTracker;
class SessionInactivityDetector;
class SessionView;
class TtcDebugSession;
class TtcKeyedService;
class TtcToolCallTracer;

class SessionControllerImpl : public SessionController,
                              public SessionViewDelegate,
                              public VoiceFocusedContentsTracker::Observer {
 public:
  explicit SessionControllerImpl(
      TtcKeyedService& service,
      std::unique_ptr<AudioController> audio_controller = nullptr);
  ~SessionControllerImpl() override;
  SessionControllerImpl(const SessionControllerImpl&) = delete;
  SessionControllerImpl& operator=(const SessionControllerImpl&) = delete;

  // SessionController and SessionViewDelegate implementation:
  Profile* GetProfile() override;
  void EndSessionAsync() override;
  SessionLifecycle GetSessionLifecycle() const override;

  // SessionController implementation:
  void AddObserver(SessionController::Observer* observer) override;
  void RemoveObserver(SessionController::Observer* observer) override;
  void SendTextInput(const std::string& text) override;
  void OnError(ErrorCode error) override;
  SessionJournalImpl& GetJournal() override;
  DebugSink* GetDebugSink() override;
  void SetSessionLifecycle(SessionLifecycle lifecycle) override;
  void SetSessionId(const std::string& session_id) override;
  void ProcessToolCall(const ToolRequest& tool_request,
                       ToolResponseCallback tool_response_callback) override;
  void ProcessToolCallBatch(std::vector<PendingToolCall> tool_calls) override;
  std::vector<ToolDefinition> GetToolDefinitions() override;
  void UserAudioLevelUpdate(float audio_level) override;
  void OnMicCaptureChanged(bool capturing) override;
  void OnSystemMicMuteChanged(bool muted, bool at_stream_start) override;
  void OnModelTurnStarted() override;
  void OnModelTurnComplete() override;
  void OnModelInterrupted() override;
  void OnInputTranscription(const std::string& text) override;
  void OnOutputTranscription(const std::string& text) override;
  void OnModelAudioEnvelopeReady(const AudioEnvelope& envelope) override;
  void OnSpeakerChanged(Speaker speaker) override;

  // SessionViewDelegate implementation:
  BrowserWindowInterface* GetBrowserWindowInterface() override;
  bool SetMicMuted(bool muted) override;
  bool IsMicMuted() const override;
  Speaker GetSpeaker() const override;

  // VoiceFocusedContentsTracker::Observer implementation:
  void OnVoiceFocusedContentsChanged(
      content::WebContents* web_contents) override;

  // The contents of the tab this session acts on, or null. Same tab the page
  // context is read from.
  content::WebContents* GetVoiceFocusedWebContents();

  actor::TaskId GetActorTaskId() const;

  // Replaces the session's actor task if it was stopped outside of the
  // session. Must be called before acting.
  void EnsureActorTaskCreated();

  // Tells observers where the get_pointer tool reported the user to be
  // pointing. See Observer::OnPointerLocationsReported().
  void NotifyPointerLocationsReported(base::span<const gfx::Point> points);

#if BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)
  // Traces tool calls for the TTC debug tools. Null unless they're enabled.
  TtcToolCallTracer* tool_call_tracer() { return tool_call_tracer_.get(); }
#endif  // BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)

  SessionView& session_view() { return CHECK_DEREF(session_view_.get()); }
  Conversation& conversation() { return CHECK_DEREF(conversation_.get()); }
  TtcPageContextMonitor* page_context_monitor_for_testing() {
    return page_context_monitor_.get();
  }

  // Where the pointer has been over the voice-focused contents, for the
  // get_pointer tool. Null unless kTtcGetPointer is enabled.
  PointerTracker* pointer_tracker() { return pointer_tracker_.get(); }

  // When the user's latest turn began: the last time they started speaking or
  // sent text, or when the session started if they have not yet.
  base::TimeTicks user_turn_start() const { return user_turn_start_; }

 private:
  // Sends the context `result` fetched for `page`, the primary page of the
  // voice-focused contents, to the conversation.
  void OnPageContextFetched(content::Page& page,
                            const PageContextResult& result);
  void OnInactivityTimeout();
  void OnSessionEndConditionDetected(
      SessionEndConditionDetector::Reason reason);

  // Notifies observers of the tool call completion and returns the response to
  // forward it to the chained callback.
  ToolResponse NotifyToolExecutionCompleted(const ToolRequest& tool_request,
                                            ToolResponse response);

  // Passes the transcripts of the current exchange on to observers.
  void NotifyTranscriptsUpdated();

  // Safe because TtcKeyedService owns this object and outlives it. Gets
  // assigned on construction.
  const raw_ref<TtcKeyedService> service_;

  // Neither is ever null.
  std::unique_ptr<Conversation> conversation_;
  std::unique_ptr<SessionView> session_view_;

  // Never null.
  std::unique_ptr<VoiceFocusedContentsTracker> voice_focused_contents_tracker_;
  base::ScopedObservation<VoiceFocusedContentsTracker,
                          VoiceFocusedContentsTracker::Observer>
      voice_focused_contents_tracker_observation_{this};

  ActorTaskHolder actor_task_holder_;
  SessionJournalImpl journal_;
#if BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)
  // The TTC debug tools' record of this session, which TtcDebugToolsService
  // owns and keeps for review after the session ends. Null unless the tools
  // are enabled. The service outlives this object (TtcKeyedServiceFactory
  // depends on its factory) and keeps the record at least until it's ended
  // here.
  raw_ptr<TtcDebugSession> debug_session_;
  // Traces tool calls into `debug_session_`; null when it is. Declared after
  // `actor_task_holder_`, which it asks for the session's actor task.
  std::unique_ptr<TtcToolCallTracer> tool_call_tracer_;
#endif  // BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)
  ToolController tool_controller_;

  // Both null unless kTtcGetPointer is enabled; the observer also while there
  // are no voice-focused contents. The observer refers to the tracker, so it
  // is declared after it.
  std::unique_ptr<PointerTracker> pointer_tracker_;
  std::unique_ptr<PointerInputObserver> pointer_input_observer_;
  base::TimeTicks user_turn_start_ = base::TimeTicks::Now();

  SessionLifecycle session_lifecycle_ = SessionLifecycle::kInitializing;
  Speaker speaker_ = Speaker::kIdle;

  // Whether the user muted the microphone in TTC, as opposed to the OS or
  // hardware muting it (see `OnSystemMicMuteChanged()`). Sessions go live
  // unmuted.
  bool app_mic_muted_ = false;

  base::ObserverList<SessionController::Observer> observers_;

  // The transcripts of the current exchange.
  TranscriptTracker transcript_tracker_;

  // Declared after `observers_` because they hold `ScopedObservation`s on
  // `this` whose destructors call `RemoveObserver()`, accessing `observers_`.
  std::unique_ptr<TtcPageContextMonitor> page_context_monitor_;
  std::unique_ptr<SessionInactivityDetector> inactivity_detector_;
  std::unique_ptr<SessionEndConditionDetector> session_end_condition_detector_;

  base::WeakPtrFactory<SessionControllerImpl> weak_ptr_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_SESSION_CONTROLLER_IMPL_H_
