// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/ttc_mes_client.h"

#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"

namespace ttc {

namespace {

optimization_guide::proto::ToolDefinition_ExecutionBehavior ToProtoBehavior(
    ToolDefinition::Behavior behavior) {
  switch (behavior) {
    case ToolDefinition::Behavior::kNonBlocking:
      return optimization_guide::proto::ToolDefinition::BEHAVIOR_NON_BLOCKING;
    case ToolDefinition::Behavior::kBlocking:
      return optimization_guide::proto::ToolDefinition::BEHAVIOR_BLOCKING;
  }
}

optimization_guide::proto::ToolDefinition_VerbalizationBehavior
ToProtoVerbalization(ToolDefinition::Verbalization verbalization) {
  switch (verbalization) {
    case ToolDefinition::Verbalization::kSilentAction:
      return optimization_guide::proto::ToolDefinition::
          VERBALIZATION_SILENT_ACTION;
    case ToolDefinition::Verbalization::kStandard:
      return optimization_guide::proto::ToolDefinition::VERBALIZATION_STANDARD;
  }
}

// Encodes `response` as the JSON object the server expects: successful calls
// report {"status": "ok"} alongside any tool-specific values, and failures
// report {"error": {"message": <message>, "code": <numeric actor error code>}},
// where each of `message` and `code` is present only if set on the error.
base::DictValue ToResponseDict(ToolResponse response) {
  if (!response.Ok()) {
    const ToolError& error = response.error();
    VLOG(1) << "Tool call failed: "
            << (error.code ? base::ToString(*error.code) : "no code") << ": "
            << error.message.value_or("no message");
    CHECK(error.message || error.code);

    base::DictValue error_dict;
    if (error.message) {
      error_dict.Set("message", *error.message);
    }
    if (error.code) {
      error_dict.Set("code", static_cast<int>(*error.code));
    }

    base::DictValue dict;
    dict.Set("error", std::move(error_dict));
    return dict;
  }

  base::DictValue dict = std::move(response).TakeResult();
  dict.Set("status", "ok");
  return dict;
}

}  // namespace

TtcMesClient::TtcMesClient(Profile* profile) : profile_(profile) {
  CHECK(profile_);
}

TtcMesClient::~TtcMesClient() {
  Close();
}

void TtcMesClient::Connect(Observer* observer) {
  CHECK(observer);

  if (session_ && is_connected_) {
    return;
  }

  observer_ = observer;

  OptimizationGuideKeyedService* opt_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
  if (!opt_guide) {
    observer_->OnStreamingStateChanged(
        false, "", "OptimizationGuideKeyedService unavailable");
    return;
  }

  optimization_guide::StreamingModelExecutionOptions options;
  options.idle_disconnect_timeout = base::Minutes(10);

  session_ = opt_guide->StartStreamingSession(
      optimization_guide::ModelBasedCapabilityKey::kTtc, options,
      base::BindRepeating(&TtcMesClient::OnStreamingResult,
                          weak_factory_.GetWeakPtr()));

  if (!session_) {
    observer_->OnStreamingStateChanged(
        false, "", "Failed to create RemoteModelExecutionSession");
    return;
  }

  session_->AddObserver(this);

  // Send initial session setup request.
  optimization_guide::proto::TtcClientFrame setup_frame;
  setup_frame.set_client_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  auto* setup = setup_frame.mutable_setup_request();
  setup->set_persona("standard");
  setup->set_audio_codec(optimization_guide::proto::AUDIO_CODEC_PCM16K16BIT);
  setup->set_enable_turn_metrics(true);
  SendFrame(setup_frame);
}

void TtcMesClient::OnConnectionStateChanged(
    optimization_guide::RemoteModelExecutionSession::ConnectionState state) {
  is_connected_ = (state == optimization_guide::RemoteModelExecutionSession::
                                ConnectionState::kConnected);
  observer_->OnStreamingStateChanged(is_connected_, session_id_, "");
}

void TtcMesClient::OnStreamingResult(
    optimization_guide::OptimizationGuideModelStreamingResult result) {
  if (!result.response.has_value()) {
    observer_->OnStreamingStateChanged(false, session_id_,
                                       base::NumberToString(static_cast<int>(
                                           result.response.error().error())));
    return;
  }

  optimization_guide::proto::TtcServerFrame server_frame;
  if (!server_frame.ParseFromString(result.response->value())) {
    return;
  }

  HandleServerFrame(server_frame);
}

void TtcMesClient::HandleServerFrame(
    const optimization_guide::proto::TtcServerFrame& frame) {
  if (frame.has_session_status()) {
    session_id_ = frame.session_status().server_session_id();
    observer_->OnStreamingStateChanged(true, session_id_, "");
  }

  if (frame.has_server_content()) {
    const auto& content = frame.server_content();
    if (content.has_audio_output()) {
      // The payload length is peer-controlled so guard that it can indeed be
      // split into 16-bit frames.
      base::span<const uint8_t> raw_bytes =
          base::as_byte_span(content.audio_output().audio_data());
      if (raw_bytes.size() % sizeof(int16_t) != 0) {
        // TODO(bokan): We should probably show an error and close down the
        // session here.
        VLOG(1) << "Discarding malformed audio chunk of " << raw_bytes.size()
                << " bytes; not a whole number of PCM16 samples";
      } else {
        observer_->OnAudioOutput(
            base::subtle::reinterpret_span<const int16_t>(raw_bytes),
            content.audio_output().sequence_number());
      }
    }

    if (!content.input_transcription().empty() ||
        !content.output_transcription().empty()) {
      observer_->OnTranscriptions(content.input_transcription(),
                                  content.output_transcription());
    }
  }

  if (frame.has_interrupted()) {
    observer_->OnGenerationStateChanged(/*started=*/false,
                                        /*completed=*/false,
                                        /*interrupted=*/true);
  }

  if (frame.has_turn_complete()) {
    observer_->OnGenerationStateChanged(/*started=*/false,
                                        /*completed=*/true,
                                        /*interrupted=*/false);
  }

  if (frame.has_tool_call()) {
    HandleToolCall(frame.tool_call());
  }

  if (frame.has_server_error()) {
    observer_->OnStreamingStateChanged(false, session_id_,
                                       frame.server_error().error_message());
  }

  if (frame.has_go_away()) {
    Close();
  }
}

void TtcMesClient::SendToolSetUpdate(const std::vector<ToolDefinition>& tools) {
  optimization_guide::proto::TtcClientFrame frame;
  frame.set_client_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  auto* update = frame.mutable_tool_set_update();
  for (const ToolDefinition& tool_def : tools) {
    auto* proto_tool = update->add_active_tools();
    proto_tool->set_name(tool_def.name);
    proto_tool->set_description(tool_def.description);
    if (!tool_def.parameters_json_schema.empty()) {
      std::optional<std::string> params_json =
          base::WriteJson(tool_def.parameters_json_schema);
      CHECK(params_json);
      proto_tool->set_parameters_json_schema(std::move(*params_json));
    }
    proto_tool->set_behavior(ToProtoBehavior(tool_def.behavior));
    proto_tool->set_verbalization(ToProtoVerbalization(tool_def.verbalization));
  }
  SendFrame(frame);
}

void TtcMesClient::HandleToolCall(
    const optimization_guide::proto::ToolCall& tool_call) {
  if (!observer_) {
    return;
  }
  ToolRequest tool_request;
  tool_request.name = tool_call.name();
  if (!tool_call.arguments_json().empty()) {
    std::optional<base::DictValue> args_dict = base::JSONReader::ReadDict(
        tool_call.arguments_json(), base::JSON_PARSE_RFC);
    if (!args_dict) {
      LOG(ERROR) << "Failed to parse tool call arguments as JSON: "
                 << tool_call.arguments_json();
      OnToolExecutionComplete(
          tool_call.call_id(), tool_call.name(),
          ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                              "Invalid JSON provided for tool arguments"));
      return;
    }
    tool_request.arguments = std::move(*args_dict);
  }

  auto callback = base::BindOnce(&TtcMesClient::OnToolExecutionComplete,
                                 weak_factory_.GetWeakPtr(),
                                 tool_call.call_id(), tool_call.name());
  observer_->OnToolCall(std::move(tool_request), std::move(callback));
}

void TtcMesClient::OnToolExecutionComplete(const std::string& call_id,
                                           const std::string& tool_name,
                                           ToolResponse response) {
  optimization_guide::proto::TtcClientFrame frame;
  frame.set_client_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  auto* tool_response = frame.mutable_tool_response();
  tool_response->set_call_id(call_id);
  tool_response->set_name(tool_name);
  std::optional<std::string> json =
      base::WriteJson(ToResponseDict(std::move(response)));
  CHECK(json);
  tool_response->set_response_json(std::move(*json));
  SendFrame(frame);
}

void TtcMesClient::SendAudioChunk(base::span<const int16_t> audio_data) {
  optimization_guide::proto::TtcClientFrame frame;
  frame.set_client_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  auto* chunk = frame.mutable_audio_input();
  base::span<const uint8_t> audio_bytes = base::as_byte_span(audio_data);
  chunk->set_audio_data(audio_bytes.data(), audio_bytes.size());
  chunk->set_timestamp_ms(base::Time::Now().InMillisecondsSinceUnixEpoch());
  SendFrame(frame);
}

void TtcMesClient::SendTextInput(const std::string& text) {
  optimization_guide::proto::TtcClientFrame frame;
  frame.set_client_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  frame.mutable_text_input()->set_text_query(text);
  SendFrame(frame);
}

void TtcMesClient::SendContextUpdate(
    const GURL& url,
    const std::string& title,
    const optimization_guide::proto::AnnotatedPageContent& apc) {
  optimization_guide::proto::TtcClientFrame frame;
  frame.set_client_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  auto* ctx = frame.mutable_context_update();
  ctx->mutable_active_tab()->set_url(url.spec());
  ctx->mutable_active_tab()->set_title(title);
  *ctx->mutable_annotated_page_content() = apc;
  SendFrame(frame);
}

void TtcMesClient::ReportPlaybackStatus(int64_t last_played_sequence_number) {
  optimization_guide::proto::TtcClientFrame frame;
  frame.set_client_timestamp_ms(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  frame.mutable_playback_status()->set_last_played_sequence_number(
      last_played_sequence_number);
  SendFrame(frame);
}

void TtcMesClient::SendFrame(
    const optimization_guide::proto::TtcClientFrame& frame) {
  if (!session_) {
    return;
  }
  session_->Send(frame);
}

void TtcMesClient::Close() {
  if (session_) {
    session_->RemoveObserver(this);
    session_.reset();
  }
  observer_ = nullptr;
  is_connected_ = false;
  session_id_.clear();
}

bool TtcMesClient::is_connected() const {
  return is_connected_;
}

}  // namespace ttc
