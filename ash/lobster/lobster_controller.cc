// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_controller.h"

#include <memory>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/lobster/lobster_session_impl.h"
#include "ash/public/cpp/lobster/lobster_client.h"
#include "ash/public/cpp/lobster/lobster_client_factory.h"
#include "ash/public/cpp/lobster/lobster_enums.h"
#include "base/command_line.h"
#include "base/hash/sha1.h"

namespace ash {
namespace {

constexpr std::string_view kLobsterKey(
    "\xB3\x3A\x4C\xFC\x84\xA0\x2B\xBE\xAC\x88\x48\x09\xCF\x5E\xD6\xD9\x28\xEC"
    "\x20\x2A",
    base::kSHA1Length);

}  // namespace

LobsterController::Trigger::Trigger(LobsterController* controller,
                                    std::unique_ptr<LobsterClient> client)
    : controller_(controller),
      client_(std::move(client)),
      state_(State::kReady) {}

LobsterController::Trigger::~Trigger() = default;

void LobsterController::Trigger::Fire(std::optional<std::string> query) {
  if (state_ == State::kDisabled) {
    return;
  }

  state_ = State::kDisabled;
  controller_->StartSession(std::move(client_), std::move(query));
}

LobsterController::LobsterController() = default;

LobsterController::~LobsterController() = default;

bool LobsterController::IsEnabled() {
  // Command line looks like:
  //  out/Default/chrome --user-data-dir=/tmp/tmp123
  //  --lobster-feature-key="INSERT KEY HERE" --enable-features=Lobster
  static const bool is_enabled =
      base::SHA1HashString(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kLobsterFeatureKey)) == kLobsterKey;
  return is_enabled;
}

void LobsterController::SetClientFactory(LobsterClientFactory* client_factory) {
  client_factory_ = client_factory;
}

std::unique_ptr<LobsterController::Trigger> LobsterController::CreateTrigger() {
  if (client_factory_ == nullptr) {
    return nullptr;
  }

  std::unique_ptr<LobsterClient> client = client_factory_->CreateClient();
  if (client == nullptr) {
    return nullptr;
  }

  LobsterSystemState system_state = client->GetSystemState();
  return system_state.status != LobsterStatus::kBlocked
             ? std::make_unique<Trigger>(this, std::move(client))
             : nullptr;
}

void LobsterController::StartSession(std::unique_ptr<LobsterClient> client,
                                     std::optional<std::string> query) {
  LobsterClient* lobster_client_ptr = client.get();
  active_session_ = std::make_unique<LobsterSessionImpl>(std::move(client));
  lobster_client_ptr->SetActiveSession(active_session_.get());

  active_session_->LoadUI(query);
}

}  // namespace ash
