// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_SESSION_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_TTC_CORE_SESSION_CONTROLLER_IMPL_H_

#include <memory>
#include <string>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ttc/app/public/audio_controller.h"
#include "chrome/browser/ttc/app/public/audio_envelope.h"
#include "chrome/browser/ttc/app/public/conversation.h"
#include "chrome/browser/ttc/core/actor_task_holder.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/session_journal_impl.h"
#include "chrome/browser/ttc/core/session_view_delegate.h"
#include "chrome/browser/ttc/core/tool_controller.h"
#include "chrome/browser/ttc/core/ttc_page_context_monitor.h"
#include "chrome/browser/ttc/core/voice_focused_contents_tracker.h"
#include "components/actor/core/task_id.h"

namespace content {
class WebContents;
}  // namespace content

namespace ttc {

class SessionView;
class TtcKeyedService;

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

  // SessionController implementation:
  void AddObserver(SessionController::Observer* observer) override;
  void RemoveObserver(SessionController::Observer* observer) override;
  void SendTextInput(const std::string& text) override;
  void OnSessionInitialized() override;
  void OnError(ErrorCode error) override;
  SessionJournalImpl& GetJournal() override;
  DebugSink* GetDebugSink() override;
  SessionLifecycle GetSessionLifecycle() const override;
  void SetSessionLifecycle(SessionLifecycle lifecycle) override;
  void ProcessToolCall(const ToolRequest& tool_request,
                       ToolResponseCallback tool_response_callback) override;
  std::vector<ToolDefinition> GetToolDefinitions() override;
  void UserAudioLevelUpdate(float audio_level) override;
  void OnModelTurnStarted() override;
  void OnModelTurnComplete() override;
  void OnModelInterrupted() override;
  void OnInputTranscription(const std::string& text) override;
  void OnOutputTranscription(const std::string& text) override;
  void OnModelAudioEnvelopeReady(const AudioEnvelope& envelope) override;

  // SessionViewDelegate implementation:
  BrowserWindowInterface* GetBrowserWindowInterface() override;

  // VoiceFocusedContentsTracker::Observer implementation:
  void OnVoiceFocusedContentsChanged(
      content::WebContents* web_contents) override;

  // The contents of the tab this session acts on, or null. Same tab the page
  // context is read from.
  content::WebContents* GetVoiceFocusedWebContents();

  actor::TaskId GetActorTaskId() const;

  // Mutes or unmutes the user's microphone. While muted, no audio is captured
  // or sent to the model. Only a live session can be muted, and it goes live
  // unmuted. Returns whether the state changed: false unless the session is
  // live, or if the microphone already was in that state.
  bool SetMicMuted(bool muted);
  // Whether the microphone of the live session is muted. False if the session
  // isn't live.
  bool IsMicMuted() const;

  SessionView& session_view() { return CHECK_DEREF(session_view_.get()); }
  Conversation& conversation() { return CHECK_DEREF(conversation_.get()); }

 private:
  void OnPageContextFetched(const PageContextResult& result);

  // Safe because TtcKeyedService owns this object and outlives it. Gets
  // assigned on construction.
  const raw_ref<TtcKeyedService> service_;

  // TODO(bokan): A ref to journal gets passed to components/ since there's no
  // interface back to SessionController yet. Therefore journal_ (which depends
  // on actor task id) needs to be destructed after these objects. We should
  // add an interface back to SessionController in components/ which would
  // remove the need for explicit destruction ordering.
  ActorTaskHolder actor_task_holder_;
  SessionJournalImpl journal_;

  // Neither is ever null.
  std::unique_ptr<Conversation> conversation_;
  std::unique_ptr<SessionView> session_view_;

  // Never null.
  std::unique_ptr<VoiceFocusedContentsTracker> voice_focused_contents_tracker_;
  base::ScopedObservation<VoiceFocusedContentsTracker,
                          VoiceFocusedContentsTracker::Observer>
      voice_focused_contents_tracker_observation_{this};

  ToolController tool_controller_;
  std::unique_ptr<TtcPageContextMonitor> page_context_monitor_;

  SessionLifecycle session_lifecycle_ = SessionLifecycle::kInitializing;

  base::ObserverList<SessionController::Observer> observers_;

  base::WeakPtrFactory<SessionControllerImpl> weak_ptr_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_SESSION_CONTROLLER_IMPL_H_
