// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/ttc_eval_harness.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"

namespace ttc {

namespace {

mojom::TurnResultPtr MakeErrorTurnResult(std::string error_message) {
  auto result = mojom::TurnResult::New();
  result->status = mojom::TurnStatus::kError;
  result->error_code = 0;
  result->error_message = std::move(error_message);
  return result;
}

}  // namespace

TtcEvalHarness::TtcEvalHarness(TtcKeyedService& service) : service_(service) {}

TtcEvalHarness::~TtcEvalHarness() {
  if (start_session_callback_) {
    start_session_timeout_.Stop();
    std::move(start_session_callback_).Run(false);
  }
  if (turn_complete_callback_) {
    if (current_turn_result_ &&
        current_turn_result_->status == mojom::TurnStatus::kComplete) {
      current_turn_result_->status = mojom::TurnStatus::kError;
      current_turn_result_->error_message =
          "Evaluation harness destroyed before turn completed";
    }
    FinishTurn();
  }
  EndSession();
}

void TtcEvalHarness::StartSession(StartSessionCallback callback) {
  if (!service_->IsEnabled() || service_->is_session_active()) {
    std::move(callback).Run(false);
    return;
  }

  start_session_callback_ = std::move(callback);
  start_session_timeout_.Start(
      FROM_HERE, base::Seconds(15),
      base::BindOnce(&TtcEvalHarness::OnStartSessionTimeout,
                     weak_factory_.GetWeakPtr()));

  auto audio = std::make_unique<EvalAudioController>();
  audio_controller_ = audio->GetWeakPtr();
  service_->StartSession(std::move(audio));

  if (SessionController* controller = service_->session_controller()) {
    observation_.Observe(controller);
  }
}

void TtcEvalHarness::EndSession() {
  if (observation_.IsObserving()) {
    observation_.Reset();
    service_->EndSession();
  }
}

void TtcEvalHarness::OnSessionLifecycleChanged(SessionLifecycle state) {
  if (state == SessionLifecycle::kLive && start_session_callback_) {
    start_session_timeout_.Stop();
    std::move(start_session_callback_).Run(true);
  } else if (state == SessionLifecycle::kFinished) {
    observation_.Reset();
    audio_controller_.reset();
    if (start_session_callback_) {
      start_session_timeout_.Stop();
      std::move(start_session_callback_).Run(false);
    }
    if (turn_complete_callback_) {
      if (current_turn_result_ &&
          current_turn_result_->status == mojom::TurnStatus::kComplete) {
        current_turn_result_->status = mojom::TurnStatus::kError;
        current_turn_result_->error_message =
            "Session ended before turn completed";
      }
      FinishTurn();
    }
  }
}

void TtcEvalHarness::OnStartSessionTimeout() {
  if (start_session_callback_) {
    std::move(start_session_callback_).Run(false);
  }
}

void TtcEvalHarness::BeginUserTurn(TurnCompleteCallback callback) {
  turn_complete_callback_ = std::move(callback);
  current_turn_result_ = mojom::TurnResult::New();
  current_turn_result_->status = mojom::TurnStatus::kComplete;
  current_turn_result_->error_code = 0;

  turn_timeout_.Start(FROM_HERE, base::Seconds(60),
                      base::BindOnce(&TtcEvalHarness::OnTurnTimeout,
                                     weak_factory_.GetWeakPtr()));
  StopAudioInjection();
}

void TtcEvalHarness::SendText(const std::string& text,
                              TurnCompleteCallback callback) {
  if (text.empty()) {
    std::move(callback).Run(MakeErrorTurnResult("Text input cannot be empty"));
    return;
  }
  SessionController* controller = observation_.GetSource();
  if (!controller ||
      controller->GetSessionLifecycle() != SessionLifecycle::kLive) {
    std::move(callback).Run(MakeErrorTurnResult("Session is not live"));
    return;
  }

  BeginUserTurn(std::move(callback));
  controller->SendTextInput(text);
}

void TtcEvalHarness::SendAudio(base::span<const int16_t> pcm16_samples,
                               TurnCompleteCallback callback) {
  if (pcm16_samples.empty()) {
    std::move(callback).Run(MakeErrorTurnResult("Audio input cannot be empty"));
    return;
  }
  SessionController* controller = observation_.GetSource();
  if (!controller ||
      controller->GetSessionLifecycle() != SessionLifecycle::kLive ||
      !audio_controller_) {
    std::move(callback).Run(MakeErrorTurnResult("Session is not live"));
    return;
  }

  BeginUserTurn(std::move(callback));

  injected_audio_.assign(pcm16_samples.begin(), pcm16_samples.end());
  injected_audio_offset_ = 0;

  audio_pump_timer_.Start(FROM_HERE, base::Milliseconds(100),
                          base::BindRepeating(&TtcEvalHarness::PumpAudio,
                                              weak_factory_.GetWeakPtr()));
}

void TtcEvalHarness::PumpAudio() {
  if (!audio_controller_) {
    StopAudioInjection();
    return;
  }

  constexpr size_t kChunkSamples = 1600;  // 100 ms at 16 kHz.
  if (injected_audio_offset_ < injected_audio_.size()) {
    size_t chunk_size = std::min(
        kChunkSamples, injected_audio_.size() - injected_audio_offset_);
    audio_controller_->InjectCapturedAudio(
        base::span(injected_audio_)
            .subspan(injected_audio_offset_, chunk_size));
    injected_audio_offset_ += chunk_size;
  } else {
    // Continuously send silence after the injected speech completes until the
    // turn finishes, matching a real microphone stream.
    std::vector<int16_t> silence(kChunkSamples, 0);
    audio_controller_->InjectCapturedAudio(silence);
  }
}

void TtcEvalHarness::StopAudioInjection() {
  audio_pump_timer_.Stop();
  injected_audio_.clear();
  injected_audio_offset_ = 0;
}

void TtcEvalHarness::OnInputTranscription(const std::string& text) {
  if (current_turn_result_) {
    current_turn_result_->input_transcription += text;
  }
}

void TtcEvalHarness::OnOutputTranscription(const std::string& text) {
  if (current_turn_result_) {
    current_turn_result_->transcription += text;
  }
}

void TtcEvalHarness::OnToolExecutionCompleted(const std::string& tool_name,
                                              bool success) {
  if (current_turn_result_) {
    auto tool_call = mojom::TurnToolCall::New();
    tool_call->name = tool_name;
    base::DictValue dict;
    if (!success) {
      dict.Set("error", true);
    }
    tool_call->arguments = base::Value(std::move(dict));
    current_turn_result_->tool_calls.push_back(std::move(tool_call));
  }
}

void TtcEvalHarness::OnError(ErrorCode error) {
  if (current_turn_result_) {
    current_turn_result_->status = mojom::TurnStatus::kError;
    current_turn_result_->error_code = static_cast<int32_t>(error);
    current_turn_result_->error_message = base::ToString(error);
  }
  FinishTurn();
}

void TtcEvalHarness::OnModelInterrupted() {
  StopAudioInjection();
}

void TtcEvalHarness::OnModelTurnComplete() {
  FinishTurn();
}

void TtcEvalHarness::FinishTurn() {
  turn_timeout_.Stop();
  StopAudioInjection();
  if (turn_complete_callback_) {
    std::move(turn_complete_callback_).Run(std::move(current_turn_result_));
  }
}

void TtcEvalHarness::OnTurnTimeout() {
  if (current_turn_result_) {
    current_turn_result_->status = mojom::TurnStatus::kTimeout;
    current_turn_result_->error_message = "Timeout waiting for turn completion";
  }
  FinishTurn();
}

}  // namespace ttc
