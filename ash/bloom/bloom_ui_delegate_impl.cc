// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/bloom/bloom_ui_delegate_impl.h"

#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "base/check.h"
#include "chromeos/components/bloom/public/cpp/bloom_interaction_resolution.h"

namespace ash {

BloomUiDelegateImpl::BloomUiDelegateImpl() = default;

BloomUiDelegateImpl::~BloomUiDelegateImpl() = default;

void BloomUiDelegateImpl::OnInteractionStarted() {
  // Nothing to do here.
}

void BloomUiDelegateImpl::OnShowUI() {
  DVLOG(1) << "Opening Assistant UI";
  if (!assistant_interaction_controller())
    return;

  assistant_interaction_controller()->StartBloomInteraction();
}

void BloomUiDelegateImpl::OnShowResult(const std::string& html) {
  if (!assistant_interaction_controller())
    return;

  assistant_interaction_controller()->ShowBloomResult("<html><body> " + html +
                                                      "</body></html>");
}

void BloomUiDelegateImpl::OnInteractionFinished(
    chromeos::bloom::BloomInteractionResolution resolution) {
  // Nothing to do here.
}

AssistantInteractionController*
BloomUiDelegateImpl::assistant_interaction_controller() {
  return AssistantInteractionController::Get();
}

}  // namespace ash
