// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_SESSION_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_TTC_CORE_SESSION_CONTROLLER_IMPL_H_

#include <string>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ttc/app/public/conversation.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/session_view_delegate.h"
#include "chrome/browser/ttc/core/tool_controller.h"
#include "chrome/browser/ttc/core/ttc_page_context_monitor.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace ttc {

class SessionView;
class TtcKeyedService;

class SessionControllerImpl : public SessionController,
                              public SessionViewDelegate,
                              public Conversation::Observer {
 public:
  explicit SessionControllerImpl(TtcKeyedService& service);
  ~SessionControllerImpl() override;
  SessionControllerImpl(const SessionControllerImpl&) = delete;
  SessionControllerImpl& operator=(const SessionControllerImpl&) = delete;

  // SessionController and SessionViewDelegate implementation:
  Profile* GetProfile() override;

  // SessionController implementation:
  void OnSessionInitialized() override;
  void GetPageContext(FetchCompleteCallback callback) override;
  void ProcessToolCall(const ToolRequest& tool_request,
                       ToolResponseCallback tool_response_callback) override;
  std::vector<ToolDefinition> GetToolDefinitions() override;
  void UserAudioLevelUpdate(float audio_level) override;

  // SessionViewDelegate implementation:
  void EndSessionAsync() override;

  // Conversation::Observer implementation:
  void OnConversationStateChanged(bool connected,
                                  const std::string& session_id,
                                  const std::string& error_message) override;

  // TODO(bokan): Android doesn't yet have a session_view so calling
  // this will crash there.
  SessionView& session_view() { return CHECK_DEREF(session_view_.get()); }
  Conversation& conversation() { return CHECK_DEREF(conversation_.get()); }

 private:
  // Returns the last active browser window for this session's profile. May be
  // null if there is no suitable window.
  BrowserWindowInterface* GetBrowserWindowInterface();

  // Returns the WebContents that the session is currently focused on and
  // observing.
  content::WebContents* GetObservedWebContents();

  // Invoked by `page_context_monitor_` when the monitored page changes.
  void OnPageContextChanged();

  // Safe because TtcKeyedService owns this object and outlives it. Gets
  // assigned on construction.
  const raw_ref<TtcKeyedService> service_;

  // Never null
  std::unique_ptr<Conversation> conversation_;
  std::unique_ptr<SessionView> session_view_;

  std::unique_ptr<TtcPageContextMonitor> page_context_monitor_;
  ToolController tool_controller_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_SESSION_CONTROLLER_IMPL_H_
