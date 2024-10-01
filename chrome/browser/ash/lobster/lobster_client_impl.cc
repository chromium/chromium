// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_client_impl.h"

#include <string>

#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "chrome/browser/ash/lobster/lobster_service.h"
#include "chrome/browser/ash/lobster/lobster_system_state_provider.h"

LobsterClientImpl::LobsterClientImpl(LobsterService* service)
    : service_(service) {}

LobsterClientImpl::~LobsterClientImpl() = default;

void LobsterClientImpl::SetActiveSession(ash::LobsterSession* session) {
  service_->SetActiveSession(session);
}

ash::LobsterSystemState LobsterClientImpl::GetSystemState() {
  return service_->system_state_provider()->GetSystemState();
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

bool LobsterClientImpl::SubmitFeedback(const std::string& query,
                                       const std::string& model_version,
                                       const std::string& description,
                                       const std::string& image_bytes) {
  return service_->SubmitFeedback(query, model_version, description,
                                  image_bytes);
}

void LobsterClientImpl::LoadUI(std::optional<std::string> query) {
  service_->LoadUI(query);
}

void LobsterClientImpl::ShowUI() {
  service_->ShowUI();
}

void LobsterClientImpl::CloseUI() {
  service_->CloseUI();
}
