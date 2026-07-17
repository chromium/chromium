// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_STREAM_PROVIDER_H_
#define CHROME_BROWSER_DICTATION_STREAM_PROVIDER_H_

#include <memory>
#include <string>

namespace dictation {

class Target;

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
  virtual void Stop() = 0;

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
