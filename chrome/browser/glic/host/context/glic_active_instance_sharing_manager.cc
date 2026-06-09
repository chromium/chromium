// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_active_instance_sharing_manager.h"

#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"

namespace glic {

GlicActiveInstanceSharingManager::GlicActiveInstanceSharingManager(
    Profile* profile,
    GlicEnabling* enabling)
    : profile_(profile) {
  CHECK(enabling);
  profile_state_subscription_ =
      enabling->RegisterProfileReadyStateChanged(base::BindRepeating(
          &GlicActiveInstanceSharingManager::OnProfileReadyStateChanged,
          base::Unretained(this)));
}

GlicActiveInstanceSharingManager::~GlicActiveInstanceSharingManager() = default;

void GlicActiveInstanceSharingManager::SetActiveSharingManager(
    GlicSharingManagerInternal* sharing_manager) {
  if (active_sharing_manager_ == sharing_manager) {
    return;
  }
  active_sharing_manager_ = sharing_manager;
  UpdateDelegate();
}

void GlicActiveInstanceSharingManager::OnProfileReadyStateChanged() {
  UpdateDelegate();
}

void GlicActiveInstanceSharingManager::UpdateDelegate() {
  if (active_sharing_manager_ &&
      GlicEnabling::IsEnabledAndConsentForProfile(profile_)) {
    SetDelegate(active_sharing_manager_);
  } else {
    SetDelegate(nullptr);
  }
}

}  // namespace glic
