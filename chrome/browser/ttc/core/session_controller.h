// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_SESSION_CONTROLLER_H_
#define CHROME_BROWSER_TTC_CORE_SESSION_CONTROLLER_H_

#include "chrome/browser/ttc/app/public/tool_types.h"
#include "chrome/browser/ttc/core/page_context.h"

class Profile;

namespace ttc {

class Conversation;

// High-level lifecycle coordinator for a TTC session. Manages the lifetime
// of the session UI (SessionView) and the model interaction (Conversation).
// Agnostic of the underlying MES transport protocol.
class SessionController {
 public:
  virtual ~SessionController() = default;

  // Called when the backend is connected and the session is interactive.
  virtual void OnSessionInitialized() = 0;

  // Fetches the context of the page this session is operating on, invoking
  // `callback` with the result.
  virtual void GetPageContext(FetchCompleteCallback callback) = 0;

  // The profile this session belongs to.
  virtual Profile* GetProfile() = 0;

  // Runs a tool (using parameters provided in `tool_request`) and calls
  // `tool_response_callback` with the result (or an error).
  virtual void ProcessToolCall(const ToolRequest& tool_request,
                               ToolResponseCallback tool_response_callback) = 0;

  // Returns a list of available tools to use for this session.
  virtual std::vector<ToolDefinition> GetToolDefinitions() = 0;

  // Called when the level (loudness) of the user's captured microphone audio
  // changes. `audio_level` is normalized to the [0, 1] range.
  virtual void UserAudioLevelUpdate(float audio_level) = 0;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_SESSION_CONTROLLER_H_
