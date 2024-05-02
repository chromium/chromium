// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/privacy_hub/content_block_observation.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/shell.h"
#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"
#include "components/prefs/pref_service.h"

namespace ash::privacy_hub_util {

ContentBlockObservation::ContentBlockObservation(
    CreationPermissionTag,
    SessionController* session_controller,
    ContentBlockCallback callback)
    : callback_(std::move(callback)), session_observation_(this) {
  session_observation_.Observe(session_controller);
}

ContentBlockObservation::~ContentBlockObservation() = default;

std::unique_ptr<ContentBlockObservation> ContentBlockObservation::Create(
    ContentBlockCallback callback) {
  auto* shell = ash::Shell::Get();
  if (!shell) {
    return nullptr;
  }
  auto* session_controller = shell->session_controller();
  if (!session_controller) {
    return nullptr;
  }
  PrefService* pref_service =
      session_controller->GetLastActiveUserPrefService();
  if (!pref_service) {
    return nullptr;
  }

  auto observation = std::make_unique<ContentBlockObservation>(
      CreationPermissionTag{}, session_controller, std::move(callback));
  observation->OnActiveUserPrefServiceChanged(pref_service);
  return observation;
}

void ContentBlockObservation::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  // Subscribing again to pref changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kUserCameraAllowed,
      base::BindRepeating(&ContentBlockObservation::OnPreferenceChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kUserMicrophoneAllowed,
      base::BindRepeating(&ContentBlockObservation::OnPreferenceChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kUserGeolocationAccessLevel,
      base::BindRepeating(&ContentBlockObservation::OnPreferenceChanged,
                          base::Unretained(this)));
}

void ContentBlockObservation::OnPreferenceChanged(
    const std::string& /*pref_name*/) {
  ContentBlock blocks;
  constexpr std::array<ContentType, 3> controllable_types{
      ContentType::MEDIASTREAM_CAMERA, ContentType::MEDIASTREAM_MIC,
      ContentType::GEOLOCATION};
  for (ContentType content_type : controllable_types) {
    if (privacy_hub_util::ContentBlocked(content_type)) {
      blocks.push_back(content_type);
    }
  }
  callback_.Run(blocks);
}

}  // namespace ash::privacy_hub_util
