// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/test/test_assistant_service.h"

#include <utility>

#include "ash/assistant/assistant_interaction_controller.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using chromeos::assistant::mojom::AssistantInteractionMetadata;
using chromeos::assistant::mojom::AssistantInteractionResolution;
using chromeos::assistant::mojom::AssistantInteractionSubscriber;
using chromeos::assistant::mojom::AssistantInteractionType;

// Subscriber that will ensure the LibAssistant contract is enforced.
// More specifically, it will ensure that:
//    - A conversation is finished before starting a new one.
//    - No responses (text, card, ...) are sent before starting or after
//    finishing an interaction.
class SanityCheckSubscriber : public AssistantInteractionSubscriber {
 public:
  SanityCheckSubscriber() : receiver_(this) {}
  ~SanityCheckSubscriber() override = default;

  mojo::PendingRemote<AssistantInteractionSubscriber>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // AssistantInteractionSubscriber implementation:
  void OnInteractionStarted(
      chromeos::assistant::mojom::AssistantInteractionMetadataPtr metadata)
      override {
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
      std::vector<chromeos::assistant::mojom::AssistantSuggestionPtr> response)
      override {
    CheckResponse();
  }

  void OnTextResponse(const std::string& response) override { CheckResponse(); }

  void OnOpenUrlResponse(const ::GURL& url, bool in_background) override {
    CheckResponse();
  }

  void OnOpenAppResponse(chromeos::assistant::mojom::AndroidAppInfoPtr app_info,
                         OnOpenAppResponseCallback callback) override {
    CheckResponse();
  }

  void OnSpeechRecognitionStarted() override {}

  void OnSpeechRecognitionIntermediateResult(
      const std::string& high_confidence_text,
      const std::string& low_confidence_text) override {}

  void OnSpeechRecognitionEndOfUtterance() override {}

  void OnSpeechRecognitionFinalResult(
      const std::string& final_result) override {}

  void OnSpeechLevelUpdated(float speech_level) override {}

  void OnTtsStarted(bool due_to_error) override {}

  void OnWaitStarted() override {}

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
  mojo::Receiver<AssistantInteractionSubscriber> receiver_;

  DISALLOW_COPY_AND_ASSIGN(SanityCheckSubscriber);
};

class InteractionResponse::Response {
 public:
  Response() = default;
  virtual ~Response() = default;

  virtual std::unique_ptr<Response> Clone() const = 0;

  virtual void SendTo(
      chromeos::assistant::mojom::AssistantInteractionSubscriber* receiver) = 0;
};

class TextResponse : public InteractionResponse::Response {
 public:
  TextResponse(const std::string& text) : text_(text) {}
  ~TextResponse() override = default;

  std::unique_ptr<Response> Clone() const override {
    return std::make_unique<TextResponse>(text_);
  }

  void SendTo(chromeos::assistant::mojom::AssistantInteractionSubscriber*
                  receiver) override {
    receiver->OnTextResponse(text_);
  }

 private:
  std::string text_;

  DISALLOW_COPY_AND_ASSIGN(TextResponse);
};

class ResolutionResponse : public InteractionResponse::Response {
 public:
  using Resolution = InteractionResponse::Resolution;

  ResolutionResponse(Resolution resolution) : resolution_(resolution) {}
  ~ResolutionResponse() override = default;

  std::unique_ptr<Response> Clone() const override {
    return std::make_unique<ResolutionResponse>(resolution_);
  }

  void SendTo(chromeos::assistant::mojom::AssistantInteractionSubscriber*
                  receiver) override {
    receiver->OnInteractionFinished(resolution_);
  }

 private:
  Resolution resolution_;

  DISALLOW_COPY_AND_ASSIGN(ResolutionResponse);
};

TestAssistantService::TestAssistantService()
    : sanity_check_subscriber_(std::make_unique<SanityCheckSubscriber>()) {
  AddAssistantInteractionSubscriber(
      sanity_check_subscriber_->BindNewPipeAndPassRemote());
}

TestAssistantService::~TestAssistantService() = default;

mojo::PendingRemote<chromeos::assistant::mojom::Assistant>
TestAssistantService::CreateRemoteAndBind() {
  return receiver_.BindNewPipeAndPassRemote();
}

void TestAssistantService::SetInteractionResponse(
    InteractionResponse&& response) {
  interaction_response_ = std::move(response);
}

void TestAssistantService::StartCachedScreenContextInteraction() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void TestAssistantService ::StartEditReminderInteraction(
    const std::string& client_id) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void TestAssistantService ::StartMetalayerInteraction(const gfx::Rect& region) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void TestAssistantService ::StartTextInteraction(
    const std::string& query,
    chromeos::assistant::mojom::AssistantQuerySource source,
    bool allow_tts) {
  StartInteraction(AssistantInteractionType::kText, source, query);
  SendInteractionResponse();
}

void TestAssistantService ::StartVoiceInteraction() {
  StartInteraction(AssistantInteractionType::kVoice);
  SendInteractionResponse();
}

void TestAssistantService ::StartWarmerWelcomeInteraction(
    int num_warmer_welcome_triggered,
    bool allow_tts) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void TestAssistantService ::StopActiveInteraction(bool cancel_conversation) {
  for (auto& subscriber : interaction_subscribers_) {
    subscriber->OnInteractionFinished(
        AssistantInteractionResolution::kInterruption);
  }
}

void TestAssistantService ::AddAssistantInteractionSubscriber(
    mojo::PendingRemote<AssistantInteractionSubscriber> subscriber) {
  interaction_subscribers_.Add(
      mojo::Remote<AssistantInteractionSubscriber>(std::move(subscriber)));
}

void TestAssistantService ::RetrieveNotification(
    chromeos::assistant::mojom::AssistantNotificationPtr notification,
    int action_index) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void TestAssistantService ::DismissNotification(
    chromeos::assistant::mojom::AssistantNotificationPtr notification) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void TestAssistantService ::CacheScreenContext(
    CacheScreenContextCallback callback) {
  std::move(callback).Run();
}

void TestAssistantService ::OnAccessibilityStatusChanged(
    bool spoken_feedback_enabled) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void TestAssistantService ::SendAssistantFeedback(
    chromeos::assistant::mojom::AssistantFeedbackPtr feedback) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void TestAssistantService::StopAlarmTimerRinging() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void TestAssistantService::CreateTimer(base::TimeDelta duration) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void TestAssistantService::StartInteraction(
    chromeos::assistant::mojom::AssistantInteractionType type,
    chromeos::assistant::mojom::AssistantQuerySource source,
    const std::string& query) {
  for (auto& subscriber : interaction_subscribers_) {
    subscriber->OnInteractionStarted(
        AssistantInteractionMetadata::New(type, source, query));
  }
}

void TestAssistantService::SendInteractionResponse() {
  InteractionResponse response = PopInteractionResponse();
  for (auto& subscriber : interaction_subscribers_)
    response.SendTo(subscriber.get());
}

InteractionResponse TestAssistantService::PopInteractionResponse() {
  return std::move(interaction_response_);
}

InteractionResponse::InteractionResponse() = default;
InteractionResponse::InteractionResponse(InteractionResponse&& other) = default;
InteractionResponse& InteractionResponse::operator=(
    InteractionResponse&& other) = default;
InteractionResponse::~InteractionResponse() = default;

InteractionResponse InteractionResponse::Clone() const {
  InteractionResponse result{};
  for (const auto& response : responses_)
    result.AddResponse(response->Clone());
  return result;
}

InteractionResponse& InteractionResponse::AddTextResponse(
    const std::string& text) {
  AddResponse(std::make_unique<TextResponse>(text));
  return *this;
}

InteractionResponse& InteractionResponse::AddResolution(Resolution resolution) {
  AddResponse(std::make_unique<ResolutionResponse>(resolution));
  return *this;
}

void InteractionResponse::AddResponse(std::unique_ptr<Response> response) {
  responses_.push_back(std::move(response));
}

void InteractionResponse::SendTo(
    chromeos::assistant::mojom::AssistantInteractionSubscriber* receiver) {
  for (auto& response : responses_)
    response->SendTo(receiver);
}

}  // namespace ash
