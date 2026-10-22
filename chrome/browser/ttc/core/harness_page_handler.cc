// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/harness_page_handler.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/ttc_eval_harness.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"

namespace ttc {

HarnessPageHandler::HarnessPageHandler(
    TtcKeyedService& service,
    mojo::PendingReceiver<mojom::HarnessPageHandler> receiver)
    : service_(&service), receiver_(this, std::move(receiver)) {}

HarnessPageHandler::~HarnessPageHandler() = default;

void HarnessPageHandler::StartSession(StartSessionCallback callback) {
  if (!service_->IsEnabled() || service_->is_session_active()) {
    std::move(callback).Run(false);
    return;
  }
  eval_harness_ = std::make_unique<TtcEvalHarness>(*service_);
  eval_harness_->StartSession(std::move(callback));
}

void HarnessPageHandler::CloseSession() {
  if (eval_harness_) {
    eval_harness_->EndSession();
    eval_harness_.reset();
  }
}

void HarnessPageHandler::SendText(const std::string& text,
                                  SendTextCallback callback) {
  if (!eval_harness_) {
    auto result = mojom::TurnResult::New();
    result->status = mojom::TurnStatus::kError;
    result->error_message = "No active session in evaluation harness";
    std::move(callback).Run(std::move(result));
    return;
  }
  eval_harness_->SendText(text, std::move(callback));
}

void HarnessPageHandler::SendAudio(const std::vector<uint8_t>& pcm16_bytes,
                                   SendAudioCallback callback) {
  if (!eval_harness_) {
    auto result = mojom::TurnResult::New();
    result->status = mojom::TurnStatus::kError;
    result->error_message = "No active session in evaluation harness";
    std::move(callback).Run(std::move(result));
    return;
  }
  if (pcm16_bytes.size() % sizeof(int16_t) != 0) {
    auto result = mojom::TurnResult::New();
    result->status = mojom::TurnStatus::kError;
    result->error_message = "PCM16 byte buffer size must be even";
    std::move(callback).Run(std::move(result));
    return;
  }
  size_t sample_count = pcm16_bytes.size() / sizeof(int16_t);
  std::vector<int16_t> audio_samples(sample_count);
  base::as_writable_byte_span(base::span(audio_samples)).copy_from(pcm16_bytes);
  eval_harness_->SendAudio(audio_samples, std::move(callback));
}

}  // namespace ttc
