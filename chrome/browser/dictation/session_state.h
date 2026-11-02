// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_SESSION_STATE_H_
#define CHROME_BROWSER_DICTATION_SESSION_STATE_H_

#include <iosfwd>

namespace dictation {

enum class SessionState {
  // Dictation is currently not active, there is no stream provider attached.
  kInactive,

  // A stream provider has just been attached but it is still starting up and
  // not yet active.
  kStreamInitializing,

  // A stream provider is attached and actively transcribing and sending
  // data.
  kTranscribing,

  // A stream provider is attached and has finished transcribing but is still
  // finalizing the transcription and more data may be provided.
  kFinalizing,

  // The session has ended and is about to be destroyed, along with any stream
  // provider. No other state can follow.
  kEnded,
};

const char* ToString(SessionState state);
std::ostream& operator<<(std::ostream& out, SessionState state);

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_SESSION_STATE_H_
