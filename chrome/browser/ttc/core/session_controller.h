// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_SESSION_CONTROLLER_H_
#define CHROME_BROWSER_TTC_CORE_SESSION_CONTROLLER_H_

#include <string>

#include "base/observer_list_types.h"
#include "chrome/browser/ttc/app/public/audio_envelope.h"
#include "chrome/browser/ttc/core/states.h"
#include "components/ttc/app/public/error_codes.h"
#include "components/ttc/app/public/tool_types.h"

class Profile;

namespace ttc {

class Conversation;
class DebugSink;
class SessionJournal;

// High-level lifecycle coordinator for a TTC session. Manages the lifetime
// of the session UI (SessionView) and the model interaction (Conversation).
// Agnostic of the underlying MES transport protocol.
class SessionController {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnSessionLifecycleChanged(SessionLifecycle lifecycle) {}
    virtual void OnModelTurnStarted() {}
    virtual void OnModelTurnComplete() {}
    virtual void OnModelInterrupted() {}
    virtual void OnError(ErrorCode error) {}
    virtual void OnToolExecutionCompleted(const std::string& tool_name,
                                          bool success) {}
    virtual void OnInputTranscription(const std::string& text) {}
    virtual void OnOutputTranscription(const std::string& text) {}
    virtual void OnModelAudioEnvelopeReady(const AudioEnvelope& envelope) {}
  };

  virtual ~SessionController() = default;

  // Registers `observer` and immediately calls `OnSessionLifecycleChanged` on
  // it with the current session lifecycle state.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Submits a user text query into the active session conversation.
  virtual void SendTextInput(const std::string& text) = 0;

  // Called when the backend is connected and the session is interactive.
  virtual void OnSessionInitialized() = 0;

  // Called when the session encounters an error.
  virtual void OnError(ErrorCode error) = 0;

  // Ends the session asynchronously
  virtual void EndSessionAsync() = 0;

  // The profile this session belongs to.
  virtual Profile* GetProfile() = 0;

  // Instrumentation for chrome://ttc-internals. Null unless the TTC debug
  // tools are enabled. Valid for the lifetime of this object.
  virtual DebugSink* GetDebugSink() = 0;

  // The journal this session's events are recorded in.
  virtual SessionJournal& GetJournal() = 0;

  // The current lifecycle state of this session.
  virtual SessionLifecycle GetSessionLifecycle() const = 0;

  // Sets the current lifecycle state of this session.
  virtual void SetSessionLifecycle(SessionLifecycle lifecycle) = 0;

  // Runs a tool (using parameters provided in `tool_request`) and calls
  // `tool_response_callback` with the result (or an error).
  virtual void ProcessToolCall(const ToolRequest& tool_request,
                               ToolResponseCallback tool_response_callback) = 0;

  // Returns a list of available tools to use for this session.
  virtual std::vector<ToolDefinition> GetToolDefinitions() = 0;

  // Called when the level (loudness) of the user's captured microphone audio
  // changes. `audio_level` is normalized to the [0, 1] range.
  virtual void UserAudioLevelUpdate(float audio_level) = 0;

  // Called by Conversation to notify about model turn lifecycle and
  // transcripts.
  virtual void OnModelTurnStarted() = 0;
  virtual void OnModelTurnComplete() = 0;
  virtual void OnModelInterrupted() = 0;
  virtual void OnInputTranscription(const std::string& text) = 0;
  virtual void OnOutputTranscription(const std::string& text) = 0;
  virtual void OnModelAudioEnvelopeReady(const AudioEnvelope& envelope) = 0;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_SESSION_CONTROLLER_H_
