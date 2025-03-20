// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/tree_fixing/internal/ax_tree_fixing_screen_ai_service.h"

#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"

namespace tree_fixing {

AXTreeFixingScreenAIService::AXTreeFixingScreenAIService(
    MainNodeIdentificationDelegate& delegate,
    Profile* profile)
    : main_node_identification_delegate_(delegate), profile_(profile) {
  CHECK(profile_);
}

AXTreeFixingScreenAIService::~AXTreeFixingScreenAIService() = default;

void AXTreeFixingScreenAIService::IdentifyMainNode(
    const ui::AXTreeUpdate& ax_tree_update,
    int request_id) {
  // Clients should not be sending requests for trees that have a kMain node.
  for (const ui::AXNodeData& node : ax_tree_update.nodes) {
    if (node.role == ax::mojom::Role::kMain) {
      NOTREACHED();
    }
  }

  // If the remote to ScreenAI has not yet been bound, do so now.
  // TODO(crbug.com/401308988): Handle cases like not ready or disconnects.
  if (!screen_ai_service_.is_bound() || !screen_ai_service_.is_connected()) {
    mojo::PendingReceiver<screen_ai::mojom::Screen2xMainContentExtractor>
        receiver = screen_ai_service_.BindNewPipeAndPassReceiver();
    screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(profile_)
        ->BindMainContentExtractor(std::move(receiver));
    screen_ai_service_.reset_on_disconnect();
    screen_ai_service_->SetClientType(
        screen_ai::mojom::MceClientType::kMainNode);
  }

  // Identify the main node using ScreenAI.
  screen_ai_service_->IdentifyMainNode(
      ax_tree_update,
      base::BindOnce(&AXTreeFixingScreenAIService::
                         ProcessScreenAIMainNodeIdentificationResult,
                     weak_ptr_factory_.GetWeakPtr(), request_id));
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
