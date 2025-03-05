// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_profile_manager.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"

namespace {
Profile* g_forced_profile_for_launch_ = nullptr;
base::MemoryPressureMonitor::MemoryPressureLevel*
    g_forced_memory_pressure_level_ = nullptr;
}  // namespace

namespace glic {
namespace {

void AutoOpenGlicPanel() {
  Profile* profile = GlicProfileManager::GetInstance()->GetProfileForLaunch();
  if (!profile) {
    return;
  }

  // TODO(379166075): Remove after updating GetProfileForLaunch.
  if (!GlicEnabling::IsEnabledForProfile(profile)) {
    return;
  }

  Browser* browser = nullptr;
  InvocationSource pretend_source = InvocationSource::kOsButton;
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ::switches::kGlicOpenOnStartup) == "attached") {
    // Attachment is best effort; FindLastActiveWithProfile() may return null
    // here.
    browser = chrome::FindLastActiveWithProfile(profile);
    pretend_source = InvocationSource::kTopChromeButton;
  }
  GlicKeyedServiceFactory::GetGlicKeyedService(profile)->ToggleUI(
      browser, /*prevent_close=*/true, pretend_source);
}

}  // namespace

GlicProfileManager* GlicProfileManager::GetInstance() {
  return g_browser_process->GetFeatures()->glic_profile_manager();
}

GlicProfileManager::GlicProfileManager() = default;

GlicProfileManager::~GlicProfileManager() = default;

Profile* GlicProfileManager::GetProfileForLaunch() const {
  if (g_forced_profile_for_launch_) {
    return g_forced_profile_for_launch_;
  }

  // If there is an active glic window open, use that profile
  if (active_glic_) {
    return active_glic_->profile();
  }

  // Look for a profile to use for glic based on order of activation
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    if (GlicEnabling::IsEnabledAndConsentForProfile(browser->profile())) {
      return browser->profile();
    }
  }

  // Look at the list of loaded profiles to use for glic
  if (g_browser_process->profile_manager()) {
    for (Profile* profile :
         g_browser_process->profile_manager()->GetLoadedProfiles()) {
      if (GlicEnabling::IsEnabledAndConsentForProfile(profile)) {
        return profile;
      }
    }
  }

  // TODO(https://crbug.com/379165457): Implement profile choice logic.
  return nullptr;
}

void GlicProfileManager::SetActiveGlic(GlicKeyedService* glic) {
  if (active_glic_ && active_glic_.get() != glic) {
    active_glic_->ClosePanel();
  }
  active_glic_ = glic->GetWeakPtr();
}

bool GlicProfileManager::ShouldPreloadForProfile(Profile* profile) const {
  if (!GlicEnabling::IsReadyForProfile(profile) ||
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

void GlicProfileManager::MaybeAutoOpenGlicPanel() {
  if (did_auto_open_ || !base::CommandLine::ForCurrentProcess()->HasSwitch(
                            ::switches::kGlicOpenOnStartup)) {
    return;
  }

  // TODO(391948342): Figure out why the FRE modal doesn't show when triggered
  // too early, and wait for that condition rather than delaying.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&AutoOpenGlicPanel), base::Seconds(5));

  did_auto_open_ = true;
}

void GlicProfileManager::ShowProfilePicker() {
  base::OnceCallback<void(Profile*)> callback = base::BindOnce(
      &GlicProfileManager::DidSelectProfile, weak_ptr_factory_.GetWeakPtr());
  // If the panel is not closed it will be on top of the profile picker.
  if (active_glic_) {
    active_glic_->ClosePanel();
  }
  ProfilePicker::Show(
      ProfilePicker::Params::ForGlicManager(std::move(callback)));
}

void GlicProfileManager::DidSelectProfile(Profile* profile) {
  // TODO(crbug.com/399727295) Remove once the profile picker calls this with
  // fully initialized profiles.
  if (!GlicEnabling::IsEnabledForProfile(profile)) {
    return;
  }
  // Toggle glic but prevent close if it is already open for the selected
  // profile.
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  service->ToggleUI(nullptr, /*prevent_close=*/true,
                    InvocationSource::kProfilePicker);
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
