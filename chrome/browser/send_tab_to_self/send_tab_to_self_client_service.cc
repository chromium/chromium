// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "chrome/browser/send_tab_to_self/desktop_notification_handler.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"

namespace send_tab_to_self {

SendTabToSelfClientService::SendTabToSelfClientService(
    std::unique_ptr<ReceivingUiHandler> receiving_ui_handler,
    SendTabToSelfModel* model)
    : receiving_ui_handler_(std::move(receiving_ui_handler)) {
  model_observation_.Observe(model);
}

SendTabToSelfClientService::~SendTabToSelfClientService() = default;

void SendTabToSelfClientService::Shutdown() {
  receiving_ui_handler_.reset();
}

void SendTabToSelfClientService::SendTabToSelfModelLoaded() {
  // TODO(crbug.com/40621767): Push changes that happened before the model was
  // loaded.
}

void SendTabToSelfClientService::EntriesAddedRemotely(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {
  if (receiving_ui_handler_) {
    receiving_ui_handler_->DisplayNewEntries(new_entries);
  }
}

void SendTabToSelfClientService::EntriesRemovedRemotely(
    const std::vector<std::string>& guids) {
  if (receiving_ui_handler_) {
    receiving_ui_handler_->DismissEntries(guids);
  }
}

ReceivingUiHandler* SendTabToSelfClientService::GetReceivingUiHandler() const {
  return receiving_ui_handler_.get();
}

}  // namespace send_tab_to_self
