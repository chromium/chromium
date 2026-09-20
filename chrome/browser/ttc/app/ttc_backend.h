// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_TTC_BACKEND_H_
#define CHROME_BROWSER_TTC_APP_TTC_BACKEND_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/ttc/app/public/tool_types.h"
#include "url/gurl.h"

namespace optimization_guide::proto {
class AnnotatedPageContent;
}  // namespace optimization_guide::proto

namespace ttc {

// Generic interface for communicating with a TTC model backend.
class TtcBackend {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnStreamingStateChanged(bool connected,
                                         const std::string& session_id,
                                         const std::string& error_message) = 0;
    virtual void OnTranscriptions(const std::string& input_transcription,
                                  const std::string& output_transcription) = 0;
    // `audio_data` holds signed 16-bit PCM samples. The buffer is only valid
    // for the duration of the call; implementations that retain the audio must
    // copy it.
    virtual void OnAudioOutput(base::span<const int16_t> audio_data,
                               int64_t sequence_number) = 0;
    virtual void OnGenerationStateChanged(bool started,
                                          bool completed,
                                          bool interrupted) = 0;
    virtual void OnToolCall(const ToolRequest& tool_request,
                            ToolResponseCallback response_callback) {}
  };

  virtual ~TtcBackend() = default;

  // Starts the streaming session with the backend. `observer` must be non-null
  // and must outlive the connection.
  virtual void Connect(Observer* observer) = 0;

  // Sends dynamic tool definitions to the backend server.
  virtual void SendToolSetUpdate(const std::vector<ToolDefinition>& tools) = 0;

  // Sends a chunk of signed PCM16 audio to the backend server.
  virtual void SendAudioChunk(base::span<const int16_t> audio_data) = 0;

  // Submits a user text query to the backend server.
  virtual void SendTextInput(const std::string& text) = 0;

  // Sends active tab context update to the backend server.
  virtual void SendContextUpdate(
      const GURL& url,
      const std::string& title,
      const optimization_guide::proto::AnnotatedPageContent& apc) = 0;

  // Reports the sequence number of audio played out to speakers.
  virtual void ReportPlaybackStatus(int64_t last_played_sequence_number) = 0;

  // Closes the active session gracefully.
  virtual void Close() = 0;

  virtual bool is_connected() const = 0;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_TTC_BACKEND_H_
