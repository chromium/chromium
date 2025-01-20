// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_service.h"

#include <string>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/lobster/lobster_session.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/hash/sha1.h"
#include "chrome/browser/ash/lobster/image_fetcher.h"
#include "chrome/browser/ash/lobster/lobster_candidate_id_generator.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "components/manta/snapper_provider.h"

LobsterService::LobsterService(
    std::unique_ptr<manta::SnapperProvider> snapper_provider,
    Profile* profile)
    : profile_(profile),
      // `LobsterService` is only created for regular profiles as specified in
      // the `LobsterServiceProvider` constructor, so the below call should
      // always return a non-null pointer.
      account_id_(CHECK_DEREF(ash::AnnotatedAccountId::Get(profile))),
      image_provider_(std::move(snapper_provider)),
      image_fetcher_(image_provider_.get(), &candidate_id_generator_),
      resizer_(&image_fetcher_),
      system_state_provider_(profile) {}

LobsterService::~LobsterService() = default;

void LobsterService::SetActiveSession(ash::LobsterSession* session) {
  active_session_ = session;
}

ash::LobsterSession* LobsterService::active_session() {
  return active_session_;
}

LobsterSystemStateProvider* LobsterService::system_state_provider() {
  return &system_state_provider_;
}

void LobsterService::RequestCandidates(
    const std::string& query,
    int num_candidates,
    ash::RequestCandidatesCallback callback) {
  image_fetcher_.RequestCandidates(query, num_candidates, std::move(callback));
}

void LobsterService::InflateCandidate(uint32_t seed,
                                      const std::string& query,
                                      ash::InflateCandidateCallback callback) {
  resizer_.InflateImage(seed, query, std::move(callback));
}

void LobsterService::QueueInsertion(const std::string& image_bytes,
                                    StatusCallback insert_status_callback) {
  queued_insertion_ = std::make_unique<LobsterInsertion>(
      image_bytes, std::move(insert_status_callback));
}

void LobsterService::LoadUI(std::optional<std::string> query,
                            ash::LobsterMode mode,
                            const gfx::Rect& caret_bounds) {
  bubble_coordinator_.LoadUI(profile_, query, mode, caret_bounds);
}

void LobsterService::ShowUI() {
  bubble_coordinator_.ShowUI();
}

void LobsterService::CloseUI() {
  bubble_coordinator_.CloseUI();
}

void LobsterService::OnFocus(int context_id) {
  if (queued_insertion_ == nullptr) {
    return;
  }

  if (queued_insertion_->HasTimedOut()) {
    queued_insertion_ = nullptr;
    return;
  }

  queued_insertion_->Commit();
  queued_insertion_ = nullptr;
}
