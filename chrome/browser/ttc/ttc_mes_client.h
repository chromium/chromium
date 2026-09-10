// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_TTC_MES_CLIENT_H_
#define CHROME_BROWSER_TTC_TTC_MES_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/proto/features/ttc.pb.h"
#include "url/gurl.h"

class Profile;

namespace ttc {

// Client for establishing and maintaining a bidirectional streaming session
// with the Model Execution Service (MES) for TTC using
// OptimizationGuideKeyedService.
class TtcMesClient
    : public optimization_guide::RemoteModelExecutionSession::Observer {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnStreamingStateChanged(bool connected,
                                         const std::string& session_id,
                                         const std::string& error_message) = 0;
    virtual void OnTranscriptions(const std::string& input_transcription,
                                  const std::string& output_transcription) = 0;
    virtual void OnAudioOutput(const std::vector<uint8_t>& audio_data,
                               int64_t sequence_number) = 0;
    virtual void OnGenerationStateChanged(bool started,
                                          bool completed,
                                          bool interrupted) = 0;
  };

  TtcMesClient(Profile* profile, Observer* observer);
  ~TtcMesClient() override;

  TtcMesClient(const TtcMesClient&) = delete;
  TtcMesClient& operator=(const TtcMesClient&) = delete;

  // Starts the streaming session with the MES backend.
  void Connect();

  // Sends raw PCM audio chunk to the MES server.
  void SendAudioChunk(const std::vector<uint8_t>& audio_data);

  // Submits a user text query to the MES server.
  void SendTextInput(const std::string& text);

  // Sends active tab context update to the MES server.
  void SendContextUpdate(
      const GURL& url,
      const std::string& title,
      const optimization_guide::proto::AnnotatedPageContent& apc);

  // Reports the sequence number of audio played out to speakers.
  void ReportPlaybackStatus(int64_t last_played_sequence_number);

  // Closes the active session gracefully.
  void Close();

  bool is_connected() const { return is_connected_; }

  // optimization_guide::RemoteModelExecutionSession::Observer:
  void OnConnectionStateChanged(
      optimization_guide::RemoteModelExecutionSession::ConnectionState state)
      override;

 private:
  void OnStreamingResult(
      optimization_guide::OptimizationGuideModelStreamingResult result);
  void HandleServerFrame(
      const optimization_guide::proto::TtcServerFrame& frame);
  void HandleToolCall(const optimization_guide::proto::ToolCall& tool_call);
  void OnToolExecutionComplete(const std::string& call_id,
                               const std::string& tool_name,
                               std::string result_json);
  void SendFrame(const optimization_guide::proto::TtcClientFrame& frame);

  raw_ptr<Profile> profile_;
  raw_ptr<Observer> observer_;

  std::unique_ptr<optimization_guide::RemoteModelExecutionSession> session_;
  bool is_connected_ = false;
  std::string session_id_;

  base::WeakPtrFactory<TtcMesClient> weak_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_TTC_MES_CLIENT_H_
