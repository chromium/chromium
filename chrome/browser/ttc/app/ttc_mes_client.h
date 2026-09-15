// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_TTC_MES_CLIENT_H_
#define CHROME_BROWSER_TTC_APP_TTC_MES_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/ttc/app/ttc_backend.h"
#include "chrome/browser/ttc/tool_definition.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/proto/features/ttc.pb.h"
#include "url/gurl.h"

class Profile;

namespace ttc {

// Client for establishing and maintaining a bidirectional streaming session
// with the Model Execution Service (MES) for TTC using
// OptimizationGuideKeyedService. Implements the TtcBackend interface.
class TtcMesClient
    : public TtcBackend,
      public optimization_guide::RemoteModelExecutionSession::Observer {
 public:
  using Observer = TtcBackend::Observer;

  TtcMesClient(Profile* profile, Observer* observer);
  ~TtcMesClient() override;

  TtcMesClient(const TtcMesClient&) = delete;
  TtcMesClient& operator=(const TtcMesClient&) = delete;

  // TtcBackend implementation:
  void set_observer(Observer* observer) override;
  void Connect() override;
  void SendToolSetUpdate(const std::vector<ToolDefinition>& tools) override;
  void SendAudioChunk(const std::vector<uint8_t>& audio_data) override;
  void SendTextInput(const std::string& text) override;
  void SendContextUpdate(
      const GURL& url,
      const std::string& title,
      const optimization_guide::proto::AnnotatedPageContent& apc) override;
  void ReportPlaybackStatus(int64_t last_played_sequence_number) override;
  void Close() override;
  bool is_connected() const override;

  // optimization_guide::RemoteModelExecutionSession::Observer:
  void OnConnectionStateChanged(
      optimization_guide::RemoteModelExecutionSession::ConnectionState state)
      override;

 protected:
  // Constructor for unit test mocks.
  TtcMesClient();

 private:
  void OnStreamingResult(
      optimization_guide::OptimizationGuideModelStreamingResult result);
  void HandleServerFrame(
      const optimization_guide::proto::TtcServerFrame& frame);
  void HandleToolCall(const optimization_guide::proto::ToolCall& tool_call);
  void OnToolExecutionComplete(const std::string& call_id,
                               const std::string& tool_name,
                               base::DictValue result);
  void SendFrame(const optimization_guide::proto::TtcClientFrame& frame);

  raw_ptr<Profile> profile_;
  raw_ptr<Observer> observer_;

  std::unique_ptr<optimization_guide::RemoteModelExecutionSession> session_;
  bool is_connected_ = false;
  std::string session_id_;

  base::WeakPtrFactory<TtcMesClient> weak_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_TTC_MES_CLIENT_H_
