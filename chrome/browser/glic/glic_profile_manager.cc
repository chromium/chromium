// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_profile_manager.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"

namespace {
Profile* g_forced_profile_for_launch_ = nullptr;
base::MemoryPressureMonitor::MemoryPressureLevel*
    g_forced_memory_pressure_level_ = nullptr;
}  // namespace

namespace glic {

GlicProfileManager* GlicProfileManager::GetInstance() {
  return g_browser_process->GetFeatures()->glic_profile_manager();
}

GlicProfileManager::GlicProfileManager() = default;

GlicProfileManager::~GlicProfileManager() = default;

void GlicProfileManager::CloseGlicWindow() {
  if (active_glic_) {
    active_glic_->ClosePanel();
    active_glic_.reset();
  }
}

Profile* GlicProfileManager::GetProfileForLaunch() const {
  if (g_forced_profile_for_launch_) {
    return g_forced_profile_for_launch_;
  }
  // TODO(https://crbug.com/379165457): Implement profile choice logic.
  // TODO(crbug.com/382722218): This needs to avoid using a profile that's been
  // disabled via enterprise policy.
  return ProfileManager::GetLastUsedProfileAllowedByPolicy();
}

void GlicProfileManager::SetActiveGlic(GlicKeyedService* glic) {
  if (active_glic_ && active_glic_.get() != glic) {
    active_glic_->ClosePanel();
  }
  active_glic_ = glic->GetWeakPtr();
}

bool GlicProfileManager::ShouldPreloadForProfile(Profile* profile) const {
  // TODO(crbug.com/390487066): Also return false when profile is not ready.
  if (!GlicEnabling::IsEnabledForProfile(profile) ||
      !base::FeatureList::IsEnabled(features::kGlicWarming)) {
    return false;
  }

  if (profile != GetProfileForLaunch()) {
    return false;
  }

  // Code below adapted from WebUIContentsPreloadManager.
  if (profile->ShutdownStarted()) {
    return false;
  }

  // Don't preload if under heavy memory pressure.
  // TODO(crbug.com/390719004): Look at discarding when pressure increases.
  if (GetCurrentPressureLevel() >=
      base::MemoryPressureMonitor::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_MODERATE) {
    return false;
  }

  return true;
}

bool GlicProfileManager::HasActiveGlicService() const {
  return active_glic_ != nullptr;
}

// static
void GlicProfileManager::ForceProfileForLaunchForTesting(Profile* profile) {
  g_forced_profile_for_launch_ = profile;
}

// static
void GlicProfileManager::ForceMemoryPressureForTesting(
    base::MemoryPressureMonitor::MemoryPressureLevel* level) {
  g_forced_memory_pressure_level_ = level;
}

base::MemoryPressureMonitor::MemoryPressureLevel
GlicProfileManager::GetCurrentPressureLevel() const {
  if (g_forced_memory_pressure_level_) {
    return *g_forced_memory_pressure_level_;
  }
  const auto* memory_monitor = base::MemoryPressureMonitor::Get();
  if (!memory_monitor) {
    return base::MemoryPressureMonitor::MemoryPressureLevel::
        MEMORY_PRESSURE_LEVEL_NONE;
  }
  return memory_monitor->GetCurrentPressureLevel();
}

}  // namespace glic
