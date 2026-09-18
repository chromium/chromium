// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_PUBLIC_CONVERSATION_H_
#define CHROME_BROWSER_TTC_APP_PUBLIC_CONVERSATION_H_

#include <string>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "url/gurl.h"

namespace optimization_guide::proto {
class AnnotatedPageContent;
}  // namespace optimization_guide::proto

namespace ttc {

// Manages a voice/multimodal conversation session with the TTC model.
// Wires audio input/output to the model backend for bidirectional streaming,
// handling speech capture, audio playback, interruptions, transcripts, and
// tools.
class Conversation {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnConversationStateChanged(bool connected,
                                            const std::string& session_id,
                                            const std::string& error_message) {}
    virtual void OnTranscriptions(const std::string& input_transcription,
                                  const std::string& output_transcription) {}
    virtual void OnGenerationStateChanged(bool started,
                                          bool completed,
                                          bool interrupted) {}
  };

  virtual ~Conversation() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Starts the conversation session: connects to backend and begins mic
  // capture.
  virtual void Start() = 0;

  // Stops the conversation session: closes backend connection and stops audio.
  virtual void Stop() = 0;

  virtual bool is_connected() const = 0;

  // Sends user text, context, or tool messages to the model.
  virtual void SendTextInput(const std::string& text) = 0;
  // TODO(bokan): Conversation should pull page context using
  // SessionController::GetPageContext. Remove this path.
  virtual void SendContextUpdate(
      const GURL& url,
      const std::string& title,
      const optimization_guide::proto::AnnotatedPageContent& apc) = 0;

  // Invoked when the page the session is operating on has changed, and so the
  // conversation's page context may be stale.
  virtual void OnPageContextChanged() = 0;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_PUBLIC_CONVERSATION_H_
