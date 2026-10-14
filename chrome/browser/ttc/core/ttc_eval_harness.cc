// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/ttc_eval_harness.h"

#include <utility>

#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"

namespace ttc {

TtcEvalHarness::TtcEvalHarness(TtcKeyedService& service) : service_(service) {}

TtcEvalHarness::~TtcEvalHarness() = default;

void TtcEvalHarness::StartSession(StartSessionCallback callback) {
  if (!service_->IsEnabled() || service_->is_session_active()) {
    std::move(callback).Run(false);
    return;
  }
  service_->StartSession();
  std::move(callback).Run(service_->is_session_active());
}

void TtcEvalHarness::EndSession() {
  service_->EndSession();
}

void TtcEvalHarness::SendText(const std::string& text,
                              TurnCompleteCallback callback) {
  auto result = mojom::TurnResult::New();
  result->status = mojom::TurnStatus::kComplete;
  result->error_code = 0;
  std::move(callback).Run(std::move(result));
}

void TtcEvalHarness::SendAudio(const std::vector<uint8_t>& pcm16_bytes,
                               TurnCompleteCallback callback) {
  auto result = mojom::TurnResult::New();
  result->status = mojom::TurnStatus::kComplete;
  result->error_code = 0;
  std::move(callback).Run(std::move(result));
}

}  // namespace ttc
