// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/essential_search/essential_search_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/session_manager/session_manager_types.h"

namespace app_list {

EssentialSearchManager::EssentialSearchManager(Profile* primary_profile)
    : primary_profile_(primary_profile) {
  scoped_observation_.Observe(ash::SessionController::Get());
}

EssentialSearchManager::~EssentialSearchManager() = default;

// static
std::unique_ptr<EssentialSearchManager> EssentialSearchManager::Create(
    Profile* primary_profile) {
  return std::make_unique<EssentialSearchManager>(primary_profile);
}

void EssentialSearchManager::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (chromeos::features::IsEssentialSearchEnabled() &&
      state == session_manager::SessionState::ACTIVE) {
    FetchSocsCookie();
  }
}

void EssentialSearchManager::FetchSocsCookie() {
  NOTIMPLEMENTED();
}

}  // namespace app_list
