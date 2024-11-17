// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_controller.h"

#include <memory>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/lobster/lobster_entry_point_enums.h"
#include "ash/lobster/lobster_session_impl.h"
#include "ash/public/cpp/lobster/lobster_client.h"
#include "ash/public/cpp/lobster/lobster_client_factory.h"
#include "ash/public/cpp/lobster/lobster_enums.h"
#include "base/command_line.h"
#include "base/hash/sha1.h"

namespace ash {

LobsterController::Trigger::Trigger(LobsterController* controller,
                                    std::unique_ptr<LobsterClient> client,
                                    LobsterEntryPoint entry_point,
                                    LobsterMode mode)
    : controller_(controller),
      client_(std::move(client)),
      state_(State::kReady),
      entry_point_(entry_point),
      mode_(mode) {}

LobsterController::Trigger::~Trigger() = default;

void LobsterController::Trigger::Fire(std::optional<std::string> query) {
  if (state_ == State::kDisabled) {
    return;
  }

  state_ = State::kDisabled;
  controller_->StartSession(std::move(client_), std::move(query), entry_point_,
                            mode_);
}

LobsterController::LobsterController() = default;

LobsterController::~LobsterController() = default;

void LobsterController::SetClientFactory(LobsterClientFactory* client_factory) {
  client_factory_ = client_factory;
}

std::unique_ptr<LobsterController::Trigger> LobsterController::CreateTrigger(
    LobsterEntryPoint entry_point,
    bool support_image_insertion) {
  if (client_factory_ == nullptr) {
    return nullptr;
  }
  std::unique_ptr<LobsterClient> client = client_factory_->CreateClient();
  if (client == nullptr || !client->UserHasAccess()) {
    return nullptr;
  }

  LobsterSystemState system_state = client->GetSystemState();
  return system_state.status != LobsterStatus::kBlocked
             ? std::make_unique<Trigger>(this, std::move(client), entry_point,
                                         support_image_insertion
                                             ? LobsterMode::kInsert
                                             : LobsterMode::kDownload)
             : nullptr;
}

void LobsterController::StartSession(std::unique_ptr<LobsterClient> client,
                                     std::optional<std::string> query,
                                     LobsterEntryPoint entry_point,
                                     LobsterMode mode) {
  if (!client->UserHasAccess()) {
    return;
  }
  // Before creating a new session, we need to inform the lobster client and
  // lobster session to clear their pointer to the session that is about to be
  // destroyed. This is to prevent them from holding a dangling pointer to the
  // old session when a new session is created.
  // TODO: crbug.com/371484317 - refactors the following logic to handle the
  // session creation better.
  client->SetActiveSession(nullptr);
  LobsterClient* lobster_client_ptr = client.get();
  active_session_ =
      std::make_unique<LobsterSessionImpl>(std::move(client), entry_point);
  lobster_client_ptr->SetActiveSession(active_session_.get());
  active_session_->LoadUI(query, mode);
}

}  // namespace ash
