// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/growth_internals/growth_internals_page_handler.h"

#include "ash/webui/growth_internals/growth_internals.mojom.h"
#include "chromeos/ash/components/growth/campaigns_logger.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"

namespace ash {

GrowthInternalsPageHandler::GrowthInternalsPageHandler(
    mojo::PendingReceiver<growth::mojom::PageHandler> pending_page_handler)
    : page_handler_(this, std::move(pending_page_handler)) {}

GrowthInternalsPageHandler::~GrowthInternalsPageHandler() = default;

void GrowthInternalsPageHandler::GetCampaignsLogs(
    GetCampaignsLogsCallback callback) {
  // `Logger` may not be initialized.
  auto* logger = ::growth::CampaignsLogger::Get();
  std::vector<std::string> logs = logger->GetLogs();
  std::move(callback).Run(logs);
}

void GrowthInternalsPageHandler::ClearAllEvents() {
  auto* campaigns_manager = ::growth::CampaignsManager::Get();
  campaigns_manager->ClearAllEvents();
}

}  // namespace ash
