// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/tree_fixing/internal/ax_tree_fixing_screen_ai_service.h"

#include <utility>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "content/public/browser/browser_thread.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"

static constexpr uint32_t kMaxInitializationAttempts = 3u;
static constexpr uint32_t kSecondsDelayOffsetBeforeReAttempt = 2u;

namespace tree_fixing {

static constexpr char kAXTreeFixingClientRequestTypeHistogramName[] =
    "Accessibility.AXTreeFixing.ScreenAI.MainNodeIdentification."
    "ClientRequestType";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AXTreeFixingClientScreenAIRequestType)
enum class AXTreeFixingClientScreenAIRequestType {
  kMainLandmarkAlreadyPresent = 0,
  kServiceNotInitialized = 1,
  kValid = 2,
  kMaxValue = kValid,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility/enums.xml:AXTreeFixingClientScreenAIRequestType)

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
  // For simplicity, if a client makes a request with a tree that already
  // contains a main node, simply return that node id.
  // TODO(401308988): Ideally clients should not be sending requests for trees
  // that have a kMain node, but until we have a UX it will make Canary testing
  // easier to not force errors. Consider adding NOTREACHED in future.
  for (const ui::AXNodeData& node : ax_tree_update.nodes) {
    if (node.role == ax::mojom::Role::kMain) {
      base::UmaHistogramEnumeration(
          kAXTreeFixingClientRequestTypeHistogramName,
          AXTreeFixingClientScreenAIRequestType::kMainLandmarkAlreadyPresent);
      main_node_identification_delegate_->OnMainNodeIdentified(
          ax_tree_update.tree_data.tree_id, node.id, request_id);
    }
  }

  // The ScreenAI service needs to be downloaded and loaded.
  if (initialization_state_ != InitializationState::kInitialized) {
    base::UmaHistogramEnumeration(
        kAXTreeFixingClientRequestTypeHistogramName,
        AXTreeFixingClientScreenAIRequestType::kServiceNotInitialized);
    NOTREACHED() << "Client sent request to identify main node before service "
                    "was ready.";
  }

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
  base::UmaHistogramEnumeration(kAXTreeFixingClientRequestTypeHistogramName,
                                AXTreeFixingClientScreenAIRequestType::kValid);
  base::UmaHistogramBoolean(
      "Accessibility.AXTreeFixing.ScreenAI.MainNodeIdentification.Request",
      true);
  screen_ai_service_->IdentifyMainNode(
      ax_tree_update,
      base::BindOnce(&AXTreeFixingScreenAIService::
                         ProcessScreenAIMainNodeIdentificationResult,
                     weak_ptr_factory_.GetWeakPtr(), request_id,
                     base::ElapsedTimer(), ax_tree_update));
}

void AXTreeFixingScreenAIService::Initialize() {
  CHECK(initialization_state_ == InitializationState::kUninitialized ||
        initialization_state_ == InitializationState::kDisconnected);
  initialization_state_ = InitializationState::kInitializing;
  ++initialization_attempt_count_;
  main_node_identification_delegate_->OnServiceStateChanged(false);
  base::UmaHistogramBoolean(
      "Accessibility.AXTreeFixing.ScreenAI.InitializationAttempt", true);
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
    base::UmaHistogramExactLinear(
        "Accessibility.AXTreeFixing.ScreenAI.InitializedOnAttempt",
        initialization_attempt_count_, kMaxInitializationAttempts);
    initialization_attempt_count_ = 0;
    initialization_state_ = InitializationState::kInitialized;
    main_node_identification_delegate_->OnServiceStateChanged(true);
  } else {
    // If this is the third consecutive time without success to re-initialize
    // the ScreenAI service, stop and send a signal to client that it failed.
    // Otherwise, re-attempt initialization.
    if (initialization_attempt_count_ >= kMaxInitializationAttempts) {
      base::UmaHistogramBoolean(
          "Accessibility.AXTreeFixing.ScreenAI.InitializedFailed", true);
      initialization_state_ = InitializationState::kInitializationFailed;
      main_node_identification_delegate_->OnServiceStateChanged(false);
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

  if (!previously_attempted_reconnect_) {
    base::UmaHistogramBoolean(
        "Accessibility.AXTreeFixing.ScreenAI.Disconnect.First", true);
    initialization_attempt_count_ = 0;
    previously_attempted_reconnect_ = true;
    Initialize();
  } else {
    base::UmaHistogramBoolean(
        "Accessibility.AXTreeFixing.ScreenAI.Disconnect.Multiple", true);
  }
}

void AXTreeFixingScreenAIService::ProcessScreenAIMainNodeIdentificationResult(
    int request_id,
    base::ElapsedTimer timer,
    const ui::AXTreeUpdate& ax_tree_update,
    const ui::AXTreeID& tree_id,
    int node_id) {
  base::UmaHistogramTimes(
      "Accessibility.AXTreeFixing.ScreenAI.MainNodeIdentification."
      "RoundTripTime",
      timer.Elapsed());
  base::UmaHistogramBoolean(
      "Accessibility.AXTreeFixing.ScreenAI.MainNodeIdentification.Response",
      true);

  bool found_main_node = node_id != ui::kInvalidAXNodeID;
  base::UmaHistogramBoolean("Accessibility.AXTreeFixing.ScreenAI.FoundMainNode",
                            found_main_node);

  if (found_main_node) {
    for (const ui::AXNodeData& node : ax_tree_update.nodes) {
      if (node.id == node_id) {
        base::UmaHistogramEnumeration(
            "Accessibility.AXTreeFixing.ScreenAI.MainNodeInitialRole",
            node.role);
        break;
      }
    }
  }

  main_node_identification_delegate_->OnMainNodeIdentified(tree_id, node_id,
                                                           request_id);
}

}  // namespace tree_fixing
