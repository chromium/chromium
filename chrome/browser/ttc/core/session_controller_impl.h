// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_SESSION_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_TTC_CORE_SESSION_CONTROLLER_IMPL_H_

#include <memory>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ttc/app/public/conversation.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/session_view_delegate.h"
#include "chrome/browser/ttc/core/tool_controller.h"
#include "chrome/browser/ttc/core/ttc_page_context_monitor.h"
#include "chrome/browser/ttc/core/voice_focused_contents_tracker.h"

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
  explicit SessionControllerImpl(TtcKeyedService& service);
  ~SessionControllerImpl() override;
  SessionControllerImpl(const SessionControllerImpl&) = delete;
  SessionControllerImpl& operator=(const SessionControllerImpl&) = delete;

  // SessionController and SessionViewDelegate implementation:
  Profile* GetProfile() override;
  void EndSessionAsync() override;

  // SessionController implementation:
  void OnSessionInitialized() override;
  void OnError(ErrorCode error) override;
  SessionJournal& GetJournal() override;
  SessionLifecycle GetSessionLifecycle() const override;
  void SetSessionLifecycle(SessionLifecycle lifecycle) override;
  void ProcessToolCall(const ToolRequest& tool_request,
                       ToolResponseCallback tool_response_callback) override;
  std::vector<ToolDefinition> GetToolDefinitions() override;
  void UserAudioLevelUpdate(float audio_level) override;

  // SessionViewDelegate implementation:
  BrowserWindowInterface* GetBrowserWindowInterface() override;

  // VoiceFocusedContentsTracker::Observer implementation:
  void OnVoiceFocusedContentsChanged(
      content::WebContents* web_contents) override;

  // The contents of the tab this session acts on, or null. Same tab the page
  // context is read from.
  content::WebContents* GetVoiceFocusedWebContents();

  SessionView& session_view() { return CHECK_DEREF(session_view_.get()); }
  Conversation& conversation() { return CHECK_DEREF(conversation_.get()); }

 private:
  void OnPageContextFetched(const PageContextResult& result);

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

  std::unique_ptr<TtcPageContextMonitor> page_context_monitor_;
  ToolController tool_controller_;

  SessionLifecycle session_lifecycle_ = SessionLifecycle::kInitializing;

  bool fatal_error_reported_ = false;

  base::WeakPtrFactory<SessionControllerImpl> weak_ptr_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_SESSION_CONTROLLER_IMPL_H_
