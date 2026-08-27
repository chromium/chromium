// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_STREAM_PROVIDER_H_
#define CHROME_BROWSER_DICTATION_STREAM_PROVIDER_H_

#include <memory>
#include <string>

namespace dictation {

class Target;

// Triggers that request or cause a Dictation stream to end.
enum class DictationStreamEndTrigger {
  kDoneButton,           // User clicked 'Done' in the bubble UI.
  kCancelButton,         // User clicked 'Cancel'/'X' in the bubble UI.
  kHotkeyToggle,         // User pressed the dictation toggle hotkey.
  kEscapeKey,            // User pressed Esc.
  kUserTyping,           // User typed text into an editable element.
  kFocusChange,          // Focus changed away or to a non-editable element.
  kNewSessionTriggered,  // A new session/stream was started elsewhere.
  // Speech recognition completed by the server (e.g. stream timeout). Currently
  // unused in normal client-driven flows where the client explicitly ends the
  // stream, but preserved for future server-side auto-endpointing
  // configurations.
  kSpeechComplete,
  kSpeechError,  // Speech recognition service error.
  kShutdown,     // Session / controller shutdown.
  kDestructor,   // Stream object destroyed before explicit stop.
  kTest,         // Test-only trigger where specific trigger is irrelevant.
};

// An interface to a Dictation StreamProvider which provides user-dicatated text
// input.
class StreamProvider {
 public:
  enum class StreamState { kInitializing, kFailed, kTranscribing, kComplete };

  virtual ~StreamProvider() = default;

  // Sets the target that the stream provider's output will be committed to, and
  // requests the stream provider to start listening and transcribing.
  virtual void BindToTargetAndConnect(std::unique_ptr<Target> target) = 0;

  // Requests the stream provider to stop listening and transcribing.
  virtual void Stop(DictationStreamEndTrigger trigger) = 0;

  // Called when transcription is updated.
  virtual void OnTranscriptionUpdated(const std::string& data,
                                      bool is_final) = 0;

  // Called when stream state changes.
  virtual void OnStreamStateChanged(StreamState state) = 0;

  // Returns the current state of the stream provider.
  virtual StreamState GetState() const = 0;

  // Returns the target that the stream provider is currently bound to.
  virtual Target* GetTarget() = 0;
  virtual const Target* GetTarget() const = 0;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_STREAM_PROVIDER_H_
