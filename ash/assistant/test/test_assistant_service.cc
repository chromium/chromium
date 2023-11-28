// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/test/test_assistant_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using assistant::AssistantInteractionMetadata;
using assistant::AssistantInteractionResolution;
using assistant::AssistantInteractionSubscriber;
using assistant::AssistantInteractionType;
using assistant::AssistantSuggestion;

// Subscriber that will ensure the LibAssistant contract is enforced.
// More specifically, it will ensure that:
//    - A conversation is finished before starting a new one.
//    - No responses (text, card, ...) are sent before starting or after
//    finishing an interaction.
class LibassistantContractChecker : public AssistantInteractionSubscriber {
 public:
  LibassistantContractChecker() = default;

  LibassistantContractChecker(const LibassistantContractChecker&) = delete;
  LibassistantContractChecker& operator=(const LibassistantContractChecker&) =
      delete;

  ~LibassistantContractChecker() override = default;

  // DefaultAssistantInteractionSubscriber implementation:
  void OnInteractionStarted(
      const AssistantInteractionMetadata& metadata) override {
    if (current_state_ == ConversationState::kInProgress) {
      ADD_FAILURE()
          << "Cannot start a new Assistant interaction without finishing the "
             "previous interaction first.";
    }
    current_state_ = ConversationState::kInProgress;
  }

  void OnInteractionFinished(
      AssistantInteractionResolution resolution) override {
    // Note: We do not check |current_state_| here as this method can be called
    // even if no interaction is in progress.
    current_state_ = ConversationState::kFinished;
  }

  void OnHtmlResponse(const std::string& response,
                      const std::string& fallback) override {
    CheckResponse();
  }

  void OnSuggestionsResponse(
      const std::vector<assistant::AssistantSuggestion>& response) override {
    CheckResponse();
  }

  void OnTextResponse(const std::string& response) override { CheckResponse(); }

  void OnOpenUrlResponse(const ::GURL& url, bool in_background) override {
    CheckResponse();
  }

  void OnOpenAppResponse(const assistant::AndroidAppInfo& app_info) override {
    CheckResponse();
  }

 private:
  void CheckResponse() {
    if (current_state_ == ConversationState::kNotStarted)
      ADD_FAILURE() << "Cannot send a response before starting an interaction.";
    if (current_state_ == ConversationState::kFinished) {
      ADD_FAILURE()
          << "Cannot send a response after finishing the interaction.";
    }
  }

  enum class ConversationState {
    kNotStarted,
    kInProgress,
    kFinished,
  };

  ConversationState current_state_ = ConversationState::kNotStarted;
};

// Subscriber that tracks the current interaction.
class CurrentInteractionSubscriber : public AssistantInteractionSubscriber {
 public:
  CurrentInteractionSubscriber() = default;
  CurrentInteractionSubscriber(CurrentInteractionSubscriber&) = delete;
  CurrentInteractionSubscriber& operator=(CurrentInteractionSubscriber&) =
      delete;
  ~CurrentInteractionSubscriber() override = default;

  // AssistantInteractionSubscriber implementation:
  void OnInteractionStarted(
      const AssistantInteractionMetadata& metadata) override {
    current_interaction_ = metadata;
  }

  void OnInteractionFinished(
      AssistantInteractionResolution resolution) override {
    current_interaction_ = std::nullopt;
  }

  std::optional<AssistantInteractionMetadata> current_interaction() {
    return current_interaction_;
  }

 private:
  std::optional<AssistantInteractionMetadata> current_interaction_ =
      std::nullopt;
};

class InteractionResponse::Response {
 public:
  Response() = default;
  virtual ~Response() = default;

  virtual void SendTo(AssistantInteractionSubscriber* receiver) = 0;
};

class TextResponse : public InteractionResponse::Response {
 public:
  explicit TextResponse(const std::string& text) : text_(text) {}

  TextResponse(const TextResponse&) = delete;
  TextResponse& operator=(const TextResponse&) = delete;

  ~TextResponse() override = default;

  void SendTo(AssistantInteractionSubscriber* receiver) override {
    receiver->OnTextResponse(text_);
  }

 private:
  std::string text_;
};

class SuggestionsResponse : public InteractionResponse::Response {
 public:
  explicit SuggestionsResponse(const std::string& text) : text_(text) {}
  SuggestionsResponse(const SuggestionsResponse&) = delete;
  SuggestionsResponse& operator=(const SuggestionsResponse&) = delete;
  ~SuggestionsResponse() override = default;

  void SendTo(AssistantInteractionSubscriber* receiver) override {
    std::vector<AssistantSuggestion> suggestions;
    suggestions.emplace_back();
    auto& suggestion = suggestions.back();
    suggestion.text = text_;
    suggestion.id = base::UnguessableToken::Create();
    receiver->OnSuggestionsResponse(suggestions);
  }

 private:
  std::string text_;
};

class ResolutionResponse : public InteractionResponse::Response {
 public:
  using Resolution = InteractionResponse::Resolution;

  explicit ResolutionResponse(Resolution resolution)
      : resolution_(resolution) {}

  ResolutionResponse(const ResolutionResponse&) = delete;
  ResolutionResponse& operator=(const ResolutionResponse&) = delete;

  ~ResolutionResponse() override = default;

  void SendTo(AssistantInteractionSubscriber* receiver) override {
    receiver->OnInteractionFinished(resolution_);
  }

 private:
  Resolution resolution_;
};

TestAssistantService::TestAssistantService()
    : libassistant_contract_checker_(
          std::make_unique<LibassistantContractChecker>()),
      current_interaction_subscriber_(
          std::make_unique<CurrentInteractionSubscriber>()) {
  AddAssistantInteractionSubscriber(libassistant_contract_checker_.get());
  AddAssistantInteractionSubscriber(current_interaction_subscriber_.get());
}

TestAssistantService::~TestAssistantService() = default;

void TestAssistantService::SetInteractionResponse(
    std::unique_ptr<InteractionResponse> response) {
  interaction_response_ = std::move(response);
}

std::optional<AssistantInteractionMetadata>
TestAssistantService::current_interaction() {
  return current_interaction_subscriber_->current_interaction();
}

void TestAssistantService::StartEditReminderInteraction(
    const std::string& client_id) {}

void TestAssistantService::StartTextInteraction(
    const std::string& query,
    assistant::AssistantQuerySource source,
    bool allow_tts) {
  StartInteraction(AssistantInteractionType::kText, source, query);
}

void TestAssistantService::StartVoiceInteraction() {
  StartInteraction(AssistantInteractionType::kVoice);
}

void TestAssistantService::StopActiveInteraction(bool cancel_conversation) {
  if (!running_active_interaction_)
    return;

  running_active_interaction_ = false;
  for (auto& subscriber : interaction_subscribers_) {
    subscriber.OnInteractionFinished(
        AssistantInteractionResolution::kInterruption);
  }
}

void TestAssistantService::AddAssistantInteractionSubscriber(
    AssistantInteractionSubscriber* subscriber) {
  interaction_subscribers_.AddObserver(subscriber);
}

void TestAssistantService::RemoveAssistantInteractionSubscriber(
    AssistantInteractionSubscriber* subscriber) {
  interaction_subscribers_.RemoveObserver(subscriber);
}

mojo::PendingReceiver<libassistant::mojom::NotificationDelegate>
TestAssistantService::GetPendingNotificationDelegate() {
  return mojo::PendingReceiver<libassistant::mojom::NotificationDelegate>();
}

void TestAssistantService::RetrieveNotification(
    const assistant::AssistantNotification& notification,
    int action_index) {}

void TestAssistantService::DismissNotification(
    const assistant::AssistantNotification& notification) {}

void TestAssistantService::OnAccessibilityStatusChanged(
    bool spoken_feedback_enabled) {}

void TestAssistantService::OnColorModeChanged(bool dark_mode_enabled) {
  dark_mode_enabled_ = dark_mode_enabled;
}

void TestAssistantService::SendAssistantFeedback(
    const assistant::AssistantFeedback& feedback) {}

void TestAssistantService::AddTimeToTimer(const std::string& id,
                                          base::TimeDelta duration) {}

void TestAssistantService::PauseTimer(const std::string& id) {}

void TestAssistantService::RemoveAlarmOrTimer(const std::string& id) {}

void TestAssistantService::ResumeTimer(const std::string& id) {}

void TestAssistantService::StartInteraction(
    assistant::AssistantInteractionType type,
    assistant::AssistantQuerySource source,
    const std::string& query) {
  if (running_active_interaction_) {
    StopActiveInteraction(/*cancel_conversation=*/false);
  }

  // Pretend to respond asynchronously.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&TestAssistantService::InteractionStarted,
                     weak_factory_.GetWeakPtr(), type, source, query));
}

void TestAssistantService::InteractionStarted(
    assistant::AssistantInteractionType type,
    assistant::AssistantQuerySource source,
    const std::string& query) {
  DCHECK(!running_active_interaction_);
  AssistantInteractionMetadata metadata{type, source, query};
  for (auto& subscriber : interaction_subscribers_) {
    subscriber.OnInteractionStarted(metadata);
  }
  running_active_interaction_ = true;

  if (interaction_response_)
    SendInteractionResponse();
}

void TestAssistantService::SendInteractionResponse() {
  DCHECK(interaction_response_);
  DCHECK(running_active_interaction_);
  for (auto& subscriber : interaction_subscribers_)
    interaction_response_->SendTo(&subscriber);
  DCHECK(!current_interaction());
  interaction_response_.reset();
  running_active_interaction_ = false;
}

InteractionResponse::InteractionResponse() = default;
InteractionResponse::~InteractionResponse() = default;

InteractionResponse* InteractionResponse::AddTextResponse(
    const std::string& text) {
  AddResponse(std::make_unique<TextResponse>(text));
  return this;
}

InteractionResponse* InteractionResponse::AddSuggestionChip(
    const std::string& text) {
  AddResponse(std::make_unique<SuggestionsResponse>(text));
  return this;
}

InteractionResponse* InteractionResponse::AddResolution(Resolution resolution) {
  AddResponse(std::make_unique<ResolutionResponse>(resolution));
  return this;
}

void InteractionResponse::AddResponse(std::unique_ptr<Response> response) {
  responses_.push_back(std::move(response));
}

void InteractionResponse::SendTo(AssistantInteractionSubscriber* receiver) {
  for (auto& response : responses_)
    response->SendTo(receiver);
}

}  // namespace ash
