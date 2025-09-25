// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_active_browser_sharing_manager.h"

#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"

namespace glic {

GlicActiveBrowserSharingManager::GlicActiveBrowserSharingManager(
    Profile* profile)
    : profile_(profile) {
  BrowserList::AddObserver(this);
}

GlicActiveBrowserSharingManager::~GlicActiveBrowserSharingManager() {
  BrowserList::RemoveObserver(this);
}

void GlicActiveBrowserSharingManager::OnBrowserSetLastActive(Browser* browser) {
  if (browser->profile() == profile_) {
    active_tab_subscription_ =
        browser->RegisterActiveTabDidChange(base::BindRepeating(
            &GlicActiveBrowserSharingManager::OnActiveTabChanged,
            base::Unretained(this)));
  } else {
    active_tab_subscription_ = {};
  }

  UpdateDelegate();
}

void GlicActiveBrowserSharingManager::OnBrowserNoLongerActive(
    Browser* browser) {
  active_tab_subscription_ = {};

  UpdateDelegate();
}

void GlicActiveBrowserSharingManager::OnActiveTabChanged(
    BrowserWindowInterface* browser) {
  UpdateDelegate();
}

void GlicActiveBrowserSharingManager::UpdateDelegate() {
  BrowserWindowInterface* const browser =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  if (browser->GetProfile() != profile_ || !browser->IsActive()) {
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

}  // namespace glic
