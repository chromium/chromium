// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_TEST_TEST_ASSISTANT_SERVICE_H_
#define ASH_ASSISTANT_TEST_TEST_ASSISTANT_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/ash/services/libassistant/public/mojom/notification_delegate.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {

class CurrentInteractionSubscriber;
class LibassistantContractChecker;

// A response issued when an Assistant interaction is started.
// Used both for text and voice interactions.  To build a response, simply
// chain calls to the provided |AddXYZ| methods.
//
// Example usage:
//     auto response = std::make_unique<InteractionResponse>();
//     response->AddTextResponse("The response text")
//             ->AddResolution(InteractionResponse::Resolution::kNormal);
//     assistant_service()->SetInteractionResponse(std::move(response));
class InteractionResponse {
 public:
  using Resolution = assistant::AssistantInteractionResolution;
  class Response;

  InteractionResponse();

  InteractionResponse(const InteractionResponse&) = delete;
  InteractionResponse& operator=(const InteractionResponse&) = delete;

  ~InteractionResponse();

  // A simple textual response.
  InteractionResponse* AddTextResponse(const std::string& text);
  // A suggestion chip response.
  InteractionResponse* AddSuggestionChip(const std::string& text);
  // If used this will cause us to finish the interaction by passing the given
  // |resolution| to |AssistantInteractionSubscriber::OnInteractionFinished|.
  InteractionResponse* AddResolution(Resolution resolution);

  void SendTo(assistant::AssistantInteractionSubscriber* receiver);

 private:
  void AddResponse(std::unique_ptr<Response> responses);

  std::vector<std::unique_ptr<Response>> responses_;
};

// Fake implementation of the Assistant service.
// It behaves as if the Assistant service is up-and-running,
// and will inform the |AssistantInteractionSubscriber| instances when
// interactions start/stop.
// Note it is up to the test developer to assure the interactions are valid.
// The contract with LibAssistant specifies there is only one
// "conversation turn" (what we call "interaction") at any given time, so you
// must:
//    - Finish a conversation before starting a new one.
//    - Not send any responses (text, card, ...) before starting or after
//      finishing an interaction.
class TestAssistantService : public assistant::Assistant {
 public:
  TestAssistantService();

  TestAssistantService(const TestAssistantService&) = delete;
  TestAssistantService& operator=(const TestAssistantService&) = delete;

  ~TestAssistantService() override;

  // Set the response that will be invoked when the next interaction starts.
  void SetInteractionResponse(std::unique_ptr<InteractionResponse> response);

  // Returns the current interaction.
  std::optional<assistant::AssistantInteractionMetadata> current_interaction();

  // Assistant overrides:
  void StartEditReminderInteraction(const std::string& client_id) override;
  void StartTextInteraction(const std::string& query,
                            assistant::AssistantQuerySource source,
                            bool allow_tts) override;
  void StartVoiceInteraction() override;
  void StopActiveInteraction(bool cancel_conversation) override;
  void AddAssistantInteractionSubscriber(
      assistant::AssistantInteractionSubscriber* subscriber) override;
  void RemoveAssistantInteractionSubscriber(
      assistant::AssistantInteractionSubscriber* subscriber) override;
  void AddRemoteConversationObserver(
      assistant::ConversationObserver* observer) override {}
  mojo::PendingReceiver<libassistant::mojom::NotificationDelegate>
  GetPendingNotificationDelegate() override;
  void RetrieveNotification(
      const assistant::AssistantNotification& notification,
      int action_index) override;
  void DismissNotification(
      const assistant::AssistantNotification& notification) override;
  void OnAccessibilityStatusChanged(bool spoken_feedback_enabled) override;
  void OnColorModeChanged(bool dark_mode_enabled) override;
  void SendAssistantFeedback(
      const assistant::AssistantFeedback& feedback) override;
  void AddTimeToTimer(const std::string& id, base::TimeDelta duration) override;
  void PauseTimer(const std::string& id) override;
  void RemoveAlarmOrTimer(const std::string& id) override;
  void ResumeTimer(const std::string& id) override;

  std::optional<bool> dark_mode_enabled() { return dark_mode_enabled_; }

 private:
  void StartInteraction(assistant::AssistantInteractionType type,
                        assistant::AssistantQuerySource source =
                            assistant::AssistantQuerySource::kUnspecified,
                        const std::string& query = std::string());
  void InteractionStarted(assistant::AssistantInteractionType type,
                          assistant::AssistantQuerySource source,
                          const std::string& query);
  void SendInteractionResponse();

  std::unique_ptr<LibassistantContractChecker> libassistant_contract_checker_;
  std::unique_ptr<CurrentInteractionSubscriber> current_interaction_subscriber_;
  std::unique_ptr<InteractionResponse> interaction_response_;

  std::optional<bool> dark_mode_enabled_;

  base::ObserverList<assistant::AssistantInteractionSubscriber>
      interaction_subscribers_;
  bool running_active_interaction_ = false;

  base::WeakPtrFactory<TestAssistantService> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_TEST_TEST_ASSISTANT_SERVICE_H_
