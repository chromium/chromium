// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_profile_manager.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace glic {

namespace {

// Ensures that the window is closed early enough (if we don't do this, we
// won't have cleaned up by the time that keyed services are destroyed).
void OnAppTerminating() {
  GlicProfileManager* mgr = GlicProfileManager::GetInstance();
  if (!mgr) {
    return;
  }
  mgr->CloseGlicWindow();
}

}  // namespace

GlicProfileManager* GlicProfileManager::GetInstance() {
  return g_browser_process->GetFeatures()->glic_profile_manager();
}

void GlicProfileManager::CloseGlicWindow() {
  if (active_glic_) {
    active_glic_->ClosePanel();
    active_glic_.reset();
  }
}

Profile* GlicProfileManager::GetProfileForLaunch() {
  // TODO(https://crbug.com/379165457): Implement profile choice logic.
  return ProfileManager::GetLastUsedProfileAllowedByPolicy();
}

void GlicProfileManager::OnUILaunching(GlicKeyedService* glic) {
  if (active_glic_ && active_glic_.get() != glic) {
    active_glic_->ClosePanel();
  }
  active_glic_ = glic->GetWeakPtr();
}

GlicProfileManager::GlicProfileManager()
    : termination_subscription_(browser_shutdown::AddAppTerminatingCallback(
          base::BindOnce(&OnAppTerminating))) {}

GlicProfileManager::~GlicProfileManager() = default;

}  // namespace glic
