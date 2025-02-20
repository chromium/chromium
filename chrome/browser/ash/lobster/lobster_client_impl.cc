// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_client_impl.h"

#include <string>
#include <utility>

#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "ash/public/cpp/lobster/lobster_text_input_context.h"
#include "chrome/browser/ash/lobster/lobster_service.h"
#include "chrome/browser/ash/lobster/lobster_system_state_provider.h"
#include "components/account_id/account_id.h"

LobsterClientImpl::LobsterClientImpl(LobsterService* service)
    : service_(service) {}

LobsterClientImpl::~LobsterClientImpl() = default;

void LobsterClientImpl::SetActiveSession(ash::LobsterSession* session) {
  service_->SetActiveSession(session);
}

ash::LobsterSystemState LobsterClientImpl::GetSystemState(
    const ash::LobsterTextInputContext& text_input_context) {
  return service_->system_state_provider()->GetSystemState(text_input_context);
}

void LobsterClientImpl::RequestCandidates(
    const std::string& query,
    int num_candidates,
    ash::RequestCandidatesCallback callback) {
  service_->RequestCandidates(query, num_candidates, std::move(callback));
}

void LobsterClientImpl::InflateCandidate(
    uint32_t seed,
    const std::string& query,
    ash::InflateCandidateCallback callback) {
  service_->InflateCandidate(seed, query, std::move(callback));
}

void LobsterClientImpl::ShowDisclaimerUI() {
  return service_->ShowDisclaimerUI();
}

void LobsterClientImpl::LoadUI(std::optional<std::string> query,
                               ash::LobsterMode mode,
                               const gfx::Rect& caret_bounds) {
  service_->LoadUI(query, mode, caret_bounds);
}

void LobsterClientImpl::ShowUI() {
  service_->ShowUI();
}

void LobsterClientImpl::CloseUI() {
  service_->CloseUI();
}

void LobsterClientImpl::QueueInsertion(const std::string& image_bytes,
                                       StatusCallback insert_status_callback) {
  service_->QueueInsertion(image_bytes, std::move(insert_status_callback));
}

const AccountId& LobsterClientImpl::GetAccountId() {
  return service_->GetAccountId();
}

void LobsterClientImpl::AnnounceLater(const std::u16string& message) {
  service_->AnnounceLater(message);
}
