// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_active_instance_sharing_manager.h"

#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/profiles/profile.h"

namespace glic {

GlicActiveInstanceSharingManager::GlicActiveInstanceSharingManager(
    Profile* profile,
    GlicEnabling* enabling,
    GlicInstanceCoordinator* instance_coordinator)
    : profile_(profile) {
  active_instance_subscription_ =
      instance_coordinator
          ->AddActiveInstanceChangedCallbackAndNotifyImmediately(
              base::BindRepeating(
                  &GlicActiveInstanceSharingManager::OnActiveInstanceChanged,
                  base::Unretained(this)));

  CHECK(enabling);
  profile_state_subscription_ =
      enabling->RegisterProfileReadyStateChanged(base::BindRepeating(
          &GlicActiveInstanceSharingManager::OnProfileReadyStateChanged,
          base::Unretained(this)));
}

GlicActiveInstanceSharingManager::~GlicActiveInstanceSharingManager() = default;

void GlicActiveInstanceSharingManager::OnActiveInstanceChanged(
    GlicInstance* instance) {
  if (GlicEnabling::IsUnifiedFreEnabled(profile_)) {
    pending_active_instance_ = instance;
    UpdateDelegate();
  } else {
    if (instance) {
      SetDelegate(&instance->host().sharing_manager());
    } else {
      SetDelegate(nullptr);
    }
  }
}

void GlicActiveInstanceSharingManager::OnProfileReadyStateChanged() {
  if (GlicEnabling::IsUnifiedFreEnabled(profile_)) {
    UpdateDelegate();
  }
}

void GlicActiveInstanceSharingManager::UpdateDelegate() {
  if (pending_active_instance_ &&
      GlicEnabling::IsEnabledAndConsentForProfile(profile_)) {
    SetDelegate(&pending_active_instance_->host().sharing_manager());
  } else {
    SetDelegate(nullptr);
  }
}

}  // namespace glic
