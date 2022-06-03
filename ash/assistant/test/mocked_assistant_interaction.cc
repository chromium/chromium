// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/test/mocked_assistant_interaction.h"

#include "ash/public/cpp/test/assistant_test_api.h"
#include "base/run_loop.h"

namespace ash {

MockedAssistantInteraction::MockedAssistantInteraction(
    AssistantTestApi* test_api,
    TestAssistantService* service)
    : test_api_(test_api),
      service_(service),
      response_(std::make_unique<InteractionResponse>()) {}

MockedAssistantInteraction::MockedAssistantInteraction(
    MockedAssistantInteraction&&) = default;

MockedAssistantInteraction& MockedAssistantInteraction::operator=(
    MockedAssistantInteraction&&) = default;

MockedAssistantInteraction::~MockedAssistantInteraction() {
  // |response_| will become nullptr if this object has been moved into another
  // instance.
  if (response_ != nullptr)
    Submit();
}

MockedAssistantInteraction& MockedAssistantInteraction::WithQuery(
    const std::string& query) {
  query_ = query;
  return *this;
}

MockedAssistantInteraction& MockedAssistantInteraction::WithTextResponse(
    const std::string& text_response) {
  response_->AddTextResponse(text_response);
  return *this;
}

MockedAssistantInteraction& MockedAssistantInteraction::WithSuggestionChip(
    const std::string& text) {
  response_->AddSuggestionChip(text);
  return *this;
}

MockedAssistantInteraction& MockedAssistantInteraction::WithResolution(
    Resolution resolution) {
  resolution_ = resolution;
  return *this;
}

void MockedAssistantInteraction::Submit() {
  DCHECK(response_ != nullptr);
  response_->AddResolution(resolution_);
  service_->SetInteractionResponse(std::move(response_));

  test_api_->SendTextQuery(query_);

  base::RunLoop().RunUntilIdle();
}

}  // namespace ash
