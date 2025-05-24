// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_profile_manager.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/network_service_instance.h"

namespace {
std::optional<Profile*> g_forced_profile_for_launch_;
std::optional<base::MemoryPressureMonitor::MemoryPressureLevel>
    g_forced_memory_pressure_level_;
std::optional<network::mojom::ConnectionType> g_forced_connection_type_;
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
  mojom::InvocationSource pretend_source = mojom::InvocationSource::kOsButton;
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ::switches::kGlicOpenOnStartup) == "attached") {
    // Attachment is best effort; FindLastActiveWithProfile() may return null
    // here.
    browser = chrome::FindLastActiveWithProfile(profile);
    pretend_source = mojom::InvocationSource::kTopChromeButton;
  }
  GlicKeyedServiceFactory::GetGlicKeyedService(profile)->ToggleUI(
      browser, /*prevent_close=*/true, pretend_source);
}

}  // namespace

GlicProfileManager* GlicProfileManager::GetInstance() {
  return g_browser_process->GetFeatures()->glic_profile_manager();
}

GlicProfileManager::GlicProfileManager() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager) {
    profile_manager->AddObserver(this);
  }
}

GlicProfileManager::~GlicProfileManager() = default;

Profile* GlicProfileManager::GetProfileForLaunch() const {
  if (g_forced_profile_for_launch_) {
    return *g_forced_profile_for_launch_;
  }

  // If the glic window is currently showing detached use that profile.
  if (last_active_glic_ && last_active_glic_->IsWindowDetached()) {
    return last_active_glic_->profile();
  }

  // Look for a profile to based on most recently used browser windows
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    if (GlicEnabling::IsEnabledAndConsentForProfile(browser->profile())) {
      return browser->profile();
    }
  }

  // TODO(https://crbug.com/379166075) Remove loaded profile look up once the
  // pinned profile is implemented.
  // Look at the list of loaded profiles to use for glic
  if (g_browser_process->profile_manager()) {
    for (Profile* profile :
         g_browser_process->profile_manager()->GetLoadedProfiles()) {
      if (GlicEnabling::IsEnabledAndConsentForProfile(profile)) {
        return profile;
      }
    }
  }

  // TODO(https://crbug.com/379166075): Implement profile choice logic.
  return nullptr;
}

void GlicProfileManager::SetActiveGlic(GlicKeyedService* glic) {
  if (last_active_glic_ && last_active_glic_.get() != glic &&
      last_active_glic_->IsWindowShowing()) {
    last_active_glic_->ClosePanel();
  }
  Profile* last_active_glic_profile = nullptr;
  if (glic) {
    last_active_glic_ = glic->GetWeakPtr();
    last_active_glic_profile = last_active_glic_->profile();
  } else {
    last_active_glic_.reset();
  }
  observers_.Notify(&Observer::OnLastActiveGlicProfileChanged,
                    last_active_glic_profile);
}

void GlicProfileManager::OnServiceShutdown(GlicKeyedService* glic) {
  if (last_active_glic_ && last_active_glic_.get() == glic) {
    SetActiveGlic(nullptr);
  }
}

void GlicProfileManager::Shutdown() {
  g_browser_process->profile_manager()->RemoveObserver(this);
}

void GlicProfileManager::OnLoadingClientForService(GlicKeyedService* glic) {
  if (base::FeatureList::IsEnabled(features::kGlicWarmMultiple)) {
    return;
  }

  if (last_loaded_glic_ && last_loaded_glic_.get() != glic) {
    last_loaded_glic_->CloseUI();
  }

  if (glic) {
    last_loaded_glic_ = glic->GetWeakPtr();
  } else {
    last_loaded_glic_.reset();
  }
}

void GlicProfileManager::OnUnloadingClientForService(GlicKeyedService* glic) {
  if (last_loaded_glic_ && last_loaded_glic_.get() == glic) {
    last_loaded_glic_.reset();
  }
}

void GlicProfileManager::ShouldPreloadForProfile(
    Profile* profile,
    ShouldPreloadCallback callback) {
  if (IsProfileDirectoryMarkedForDeletion(profile->GetPath())) {
    return;
  }
  if (!base::FeatureList::IsEnabled(features::kGlicWarming) ||
      !GlicEnabling::IsReadyForProfile(profile)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), profile, false));
    return;
  }
  CanPreloadForProfile(profile, std::move(callback));
}

void GlicProfileManager::ShouldPreloadFreForProfile(
    Profile* profile,
    ShouldPreloadCallback callback) {
  if (!base::FeatureList::IsEnabled(features::kGlicFreWarming) ||
      // We only want to preload the FRE if it has not been completed.
      GlicEnabling::IsEnabledAndConsentForProfile(profile)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), profile, false));
    return;
  }
  CanPreloadForProfile(profile, std::move(callback));
}

GlicKeyedService* GlicProfileManager::GetLastActiveGlic() const {
  return last_active_glic_.get();
}

void GlicProfileManager::MaybeAutoOpenGlicPanel() {
  if (did_auto_open_ || !base::CommandLine::ForCurrentProcess()->HasSwitch(
                            ::switches::kGlicOpenOnStartup)) {
    return;
  }

  // TODO(391948342): Figure out why the FRE modal doesn't show when triggered
  // too early, and wait for that condition rather than delaying.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&AutoOpenGlicPanel), base::Seconds(30));

  did_auto_open_ = true;
}

void GlicProfileManager::ShowProfilePicker() {
  base::OnceCallback<void(Profile*)> callback = base::BindOnce(
      &GlicProfileManager::DidSelectProfile, weak_ptr_factory_.GetWeakPtr());
  // If the panel is not closed it will be on top of the profile picker.
  if (last_active_glic_) {
    last_active_glic_->ClosePanel();
  }
  ProfilePicker::Show(
      ProfilePicker::Params::ForGlicManager(std::move(callback)));
}

void GlicProfileManager::DidSelectProfile(Profile* profile) {
  if (!GlicEnabling::IsEnabledForProfile(profile)) {
    return;
  }

  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile);

  if (!GlicEnabling::HasConsentedForProfile(profile)) {
    // Open a browser and show the FRE in a new tab.
    chrome::ScopedTabbedBrowserDisplayer displayer(profile);
    service->OpenFreDialogInNewTab(displayer.browser());
  } else {
    // Toggle glic but prevent close if it is already open for the selected
    // profile.
    service->ToggleUI(nullptr, /*prevent_close=*/true,
                      mojom::InvocationSource::kProfilePicker);
  }
}

void GlicProfileManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GlicProfileManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool GlicProfileManager::IsShowing() const {
  if (!last_active_glic_) {
    return false;
  }
  return last_active_glic_->window_controller().IsPanelOrFreShowing();
}

void GlicProfileManager::OnProfileMarkedForPermanentDeletion(Profile* profile) {
  GlicKeyedService* glic_keyed_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  if (!glic_keyed_service) {
    return;
  }
  glic_keyed_service->Shutdown();
}

// static
void GlicProfileManager::ForceProfileForLaunchForTesting(
    std::optional<Profile*> profile) {
  g_forced_profile_for_launch_ = profile;
}

// static
void GlicProfileManager::ForceMemoryPressureForTesting(
    std::optional<base::MemoryPressureMonitor::MemoryPressureLevel> level) {
  g_forced_memory_pressure_level_ = level;
}

// static
void GlicProfileManager::ForceConnectionTypeForTesting(
    std::optional<network::mojom::ConnectionType> connection_type) {
  g_forced_connection_type_ = connection_type;
}

bool GlicProfileManager::IsUnderMemoryPressure() const {
  // TODO(crbug.com/390719004): Look at discarding when pressure increases.
  base::MemoryPressureMonitor::MemoryPressureLevel memory_pressure = base::
      MemoryPressureMonitor::MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_NONE;
  if (g_forced_memory_pressure_level_) {
    memory_pressure = *g_forced_memory_pressure_level_;
  } else if (const auto* memory_monitor = base::MemoryPressureMonitor::Get()) {
    memory_pressure = memory_monitor->GetCurrentPressureLevel();
  }
  return memory_pressure >= base::MemoryPressureMonitor::MemoryPressureLevel::
                                MEMORY_PRESSURE_LEVEL_MODERATE;
}

void GlicProfileManager::CanPreloadForProfile(Profile* profile,
                                              ShouldPreloadCallback callback) {
  const bool is_last_active =
      last_active_glic_ && last_active_glic_->profile() == profile;
  const bool is_last_loaded =
      last_loaded_glic_ && last_loaded_glic_->profile() == profile;
  const bool blocked_by_shown_glic =
      !base::FeatureList::IsEnabled(features::kGlicWarmMultiple) && IsShowing();

  if (!profile || !GlicEnabling::IsEnabledForProfile(profile) ||
      is_last_loaded || is_last_active || blocked_by_shown_glic ||
      profile->ShutdownStarted() || IsUnderMemoryPressure()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), profile, false));
    return;
  }

  auto on_got_connection_type = [](Profile* profile,
                                   ShouldPreloadCallback callback,
                                   network::mojom::ConnectionType type) {
    std::move(callback).Run(
        profile,
        !network::NetworkConnectionTracker::IsConnectionCellular(type));
  };
  auto callbacks = base::SplitOnceCallback(std::move(callback));

  // Attempt to synchronously query the connection type.
  network::mojom::ConnectionType connection_type;
  bool synchronously_got_connection_type = false;
  if (g_forced_connection_type_) {
    synchronously_got_connection_type = true;
    connection_type = *g_forced_connection_type_;
  } else {
    synchronously_got_connection_type =
        content::GetNetworkConnectionTracker()->GetConnectionType(
            &connection_type, base::BindOnce(on_got_connection_type, profile,
                                             std::move(callbacks.first)));
  }

  if (synchronously_got_connection_type) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(on_got_connection_type, profile,
                       std::move(callbacks.second), connection_type));
  }
}

base::WeakPtr<GlicProfileManager> GlicProfileManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace glic
