// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_active_browser_sharing_manager.h"

#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace glic {

GlicActiveBrowserSharingManager::GlicActiveBrowserSharingManager(
    Profile* profile)
    : profile_(profile) {}
GlicActiveBrowserSharingManager::~GlicActiveBrowserSharingManager() = default;

void GlicActiveBrowserSharingManager::OnBrowserSetLastActive(Browser* browser) {
  if (browser->profile() != profile_) {
    SetDelegate(nullptr);
    return;
  }

  GlicInstance* glic_instance =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile_)
          ->GetInstanceForActiveTab(browser);

  if (glic_instance && glic_instance->IsShowing()) {
    SetDelegate(glic_instance->host().sharing_manager().GetWeakPtr());
    return;
  }

  SetDelegate(nullptr);
}

void GlicActiveBrowserSharingManager::OnBrowserNoLongerActive(
    Browser* browser) {
  SetDelegate(nullptr);
}

}  // namespace glic
