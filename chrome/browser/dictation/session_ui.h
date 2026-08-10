// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_SESSION_UI_H_
#define CHROME_BROWSER_DICTATION_SESSION_UI_H_

#include "content/public/browser/global_dom_node_id.h"

namespace dictation {

// Interface for the view controller of browser-level UI behavior.
class SessionUi {
 public:
  enum class StreamType { kAttached, kFinalizing };

  virtual ~SessionUi() = default;

  // Called when a stream encounters an error.
  virtual void OnError(StreamType stream_type) = 0;

  // Called when the dictation session has stopped.
  virtual void OnStopped() = 0;

  // Called when the audio level is updated.
  virtual void UpdateAudioLevel(float audio_level) = 0;

  // Called when a stream is started on `target_id`.
  virtual void OnStartedStream(content::GlobalDOMNodeId target_id) = 0;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_SESSION_UI_H_
