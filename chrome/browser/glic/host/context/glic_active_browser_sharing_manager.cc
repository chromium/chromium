// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_active_browser_sharing_manager.h"

#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/profiles/profile.h"

namespace glic {

GlicActiveBrowserSharingManager::GlicActiveBrowserSharingManager(
    Profile* profile,
    GlicInstanceCoordinator* instance_coordinator)
    : active_tab_tracker_(profile), profile_(profile) {
  active_tab_subscription_ = active_tab_tracker_.AddActiveTabChangedCallback(
      base::BindRepeating(&GlicActiveBrowserSharingManager::OnActiveTabChanged,
                          base::Unretained(this)));

  last_active_instance_subscription_ =
      instance_coordinator->RegisterLastActiveInstanceChangedCallback(
          base::BindRepeating(
              &GlicActiveBrowserSharingManager::OnLastActiveInstanceChanged,
              base::Unretained(this)));
}

GlicActiveBrowserSharingManager::~GlicActiveBrowserSharingManager() = default;

void GlicActiveBrowserSharingManager::OnActiveTabChanged(
    tabs::TabInterface* active_tab) {
  UpdateDelegate();
}

void GlicActiveBrowserSharingManager::OnLastActiveInstanceChanged(
    GlicInstance* instance) {
  // We listen for this to trigger when the SP is opened or closed, or when the
  // conversation switches, but we rely on UpdateDelegate to take all signals
  // into consideration (e.g. active tab) at any given moment.
  UpdateDelegate();
}

void GlicActiveBrowserSharingManager::UpdateDelegate() {
  tabs::TabInterface* active_tab = active_tab_tracker_.GetActiveTab();
  if (!active_tab) {
    SetDelegate(nullptr);
    return;
  }

  GlicInstance* glic_instance =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_)->GetInstanceForTab(
          active_tab);
  if (glic_instance && glic_instance->IsShowing()) {
    SetDelegate(glic_instance->host().sharing_manager().GetWeakPtr());
    return;
  }

  SetDelegate(nullptr);
}

}  // namespace glic
