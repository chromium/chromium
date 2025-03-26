// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/tree_fixing/internal/ax_tree_fixing_screen_ai_service.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "content/public/browser/browser_thread.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"

static constexpr uint32_t kMaxInitializationAttempts = 3u;
static constexpr uint32_t kSecondsDelayOffsetBeforeReAttempt = 2u;

namespace tree_fixing {

AXTreeFixingScreenAIService::AXTreeFixingScreenAIService(
    MainNodeIdentificationDelegate& delegate,
    Profile* profile)
    : main_node_identification_delegate_(delegate), profile_(profile) {
  CHECK(profile_);
  Initialize();
}

AXTreeFixingScreenAIService::~AXTreeFixingScreenAIService() = default;

void AXTreeFixingScreenAIService::IdentifyMainNode(
    const ui::AXTreeUpdate& ax_tree_update,
    int request_id) {
  // Client should not be sending requests for trees that have a kMain node.
  for (const ui::AXNodeData& node : ax_tree_update.nodes) {
    if (node.role == ax::mojom::Role::kMain) {
      NOTREACHED() << "A node with the main landmark is already present in the "
                      "accessibility tree.";
    }
  }

  // The ScreenAI service needs to be downloaded and loaded.
  CHECK_EQ(initialization_state_, InitializationState::kInitialized)
      << "Client sent request to identify main node before service was ready";

  // If the remote to ScreenAI has not yet been bound, do so now.
  if (!screen_ai_service_.is_bound() || !screen_ai_service_.is_connected()) {
    mojo::PendingReceiver<screen_ai::mojom::Screen2xMainContentExtractor>
        receiver = screen_ai_service_.BindNewPipeAndPassReceiver();
    screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(profile_)
        ->BindMainContentExtractor(std::move(receiver));
    screen_ai_service_->SetClientType(
        screen_ai::mojom::MceClientType::kMainNode);
    screen_ai_service_.set_disconnect_handler(
        base::BindOnce(&AXTreeFixingScreenAIService::HandleServiceDisconnect,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Identify the main node using ScreenAI.
  screen_ai_service_->IdentifyMainNode(
      ax_tree_update,
      base::BindOnce(&AXTreeFixingScreenAIService::
                         ProcessScreenAIMainNodeIdentificationResult,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
}

void AXTreeFixingScreenAIService::Initialize() {
  CHECK(initialization_state_ == InitializationState::kUninitialized ||
        initialization_state_ == InitializationState::kDisconnected);
  initialization_state_ = InitializationState::kInitializing;
  ++initialization_attempt_count_;
  main_node_identification_delegate_->OnServiceStateChanged(false);
  screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(profile_)
      ->GetServiceStateAsync(
          screen_ai::ScreenAIServiceRouter::Service::kMainContentExtraction,
          base::BindOnce(
              &AXTreeFixingScreenAIService::ServiceInitializationCallback,
              weak_ptr_factory_.GetWeakPtr()));
}

void AXTreeFixingScreenAIService::ServiceInitializationCallback(
    bool successful) {
  if (successful) {
    initialization_attempt_count_ = 0;
    initialization_state_ = InitializationState::kInitialized;
    main_node_identification_delegate_->OnServiceStateChanged(true);
  } else {
    // If this is the third consecutive time without success to re-initialize
    // the ScreenAI service, stop and send a signal to client that it failed.
    // Otherwise, re-attempt initialization.
    if (initialization_attempt_count_ >= kMaxInitializationAttempts) {
      initialization_state_ = InitializationState::kInitializationFailed;
      main_node_identification_delegate_->OnServiceStateChanged(false);
      // TODO(crbug.com/399383663): Record metric for repeated failures?
    } else {
      initialization_state_ = InitializationState::kUninitialized;
      // The ScreenAI service is suspended internally after each crash. We will
      // wait for that amount of time with a small padding to ensure we
      // re-attempt only after ScreenAI's suspension.
      base::TimeDelta delay =
          screen_ai::ScreenAIServiceRouter::SuggestedWaitTimeBeforeReAttempt(
              initialization_attempt_count_);
      delay += base::Seconds(kSecondsDelayOffsetBeforeReAttempt);
      content::GetUIThreadTaskRunner({})->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&AXTreeFixingScreenAIService::Initialize,
                         weak_ptr_factory_.GetWeakPtr()),
          delay);
    }
  }
}

void AXTreeFixingScreenAIService::HandleServiceDisconnect() {
  // On the first disconnect of the service, we will attempt to reconnect.
  // Inform client the service is no longer ready, and reconnect.
  initialization_state_ = InitializationState::kDisconnected;
  main_node_identification_delegate_->OnServiceStateChanged(false);

  // TODO(crbug.com/399383663): Record metric for disconnects?
  if (!previously_attempted_reconnect_) {
    initialization_attempt_count_ = 0;
    previously_attempted_reconnect_ = true;
    Initialize();
  }
}

void AXTreeFixingScreenAIService::ProcessScreenAIMainNodeIdentificationResult(
    const ui::AXTreeID& tree_id,
    int node_id,
    int request_id) {
  // TODO(crbug.com/399383663): Add metrics, internal logic, etc.
  main_node_identification_delegate_->OnMainNodeIdentified(tree_id, node_id,
                                                           request_id);
}

}  // namespace tree_fixing
