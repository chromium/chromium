// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_profile_manager.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/network_service_instance.h"

namespace {
std::optional<Profile*> g_forced_profile_for_launch_;
std::optional<base::MemoryPressureLevel> g_forced_memory_pressure_level_;
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

  // If the glic window is currently showing detached use that profile. When
  // GlicMultiInstance is enabled, this profile is the one where a detached
  // instance was most recently used.
  if (!GlicEnabling::IsMultiInstanceEnabled() && last_active_glic_ &&
      last_active_glic_->IsWindowDetached()) {
    return last_active_glic_->profile();
  } else if (GlicEnabling::IsMultiInstanceEnabled() && current_detached_glic_) {
    return current_detached_glic_->profile();
  }

  // Look for a profile to based on most recently used browser windows
  Profile* profile_from_browser_window = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (GlicEnabling::IsEnabledAndConsentForProfile(
                browser->GetProfile())) {
          profile_from_browser_window = browser->GetProfile();
          return false;  // stop iterating
        }
        return true;  // continue iterating
      });
  if (profile_from_browser_window != nullptr) {
    return profile_from_browser_window;
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
    // This is only relevant to single-instance glic, as IsWindowShowing remains
    // unimplemented in multi-instance.
    last_active_glic_->window_controller().Close();
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

void GlicProfileManager::SetCurrentDetachedGlic(Profile* profile) {
  if (!profile) {
    current_detached_glic_.reset();
    return;
  }
  if (current_detached_glic_ && current_detached_glic_->profile() != profile) {
    current_detached_glic_->window_controller().Close();
  }
  current_detached_glic_ = GlicKeyedService::Get(profile)->GetWeakPtr();
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

  if (last_loaded_glic_ && last_loaded_glic_.get() != glic &&
      !GlicEnabling::IsMultiInstanceEnabled()) {
    last_loaded_glic_->CloseAndShutdown();
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
  if (!base::FeatureList::IsEnabled(features::kGlicWarming)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       GlicPrewarmingChecksResult::kWarmingDisabled));
    return;
  }
  GlicPrewarmingChecksResult result;
  switch (GlicEnabling::GetProfileReadyState(profile)) {
    case mojom::ProfileReadyState::kReady:
      CanPreloadForProfile(profile, std::move(callback));
      return;
    case mojom::ProfileReadyState::kUnknownError:
      result = GlicPrewarmingChecksResult::kProfileNotReadyUnknown;
      break;
    case mojom::ProfileReadyState::kSignInRequired:
      result = GlicPrewarmingChecksResult::kProfileRequiresSignIn;
      break;
    case mojom::ProfileReadyState::kIneligible:
      result = GlicPrewarmingChecksResult::kProfileNotEligible;
      break;
    case mojom::ProfileReadyState::kDisabledByAdmin:
      result = GlicPrewarmingChecksResult::kProfileDisallowedByAdmin;
      break;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void GlicProfileManager::ShouldPreloadFreForProfile(
    Profile* profile,
    ShouldPreloadCallback callback) {
  if (!base::FeatureList::IsEnabled(features::kGlicFreWarming)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       GlicPrewarmingChecksResult::kWarmingDisabled));
    return;
  }
  if (GlicEnabling::IsEnabledAndConsentForProfile(profile)) {
    // We only want to preload the FRE if it has not been completed.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       GlicPrewarmingChecksResult::kUserAlreadyWentTroughFre));
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
    last_active_glic_->window_controller().Close();
  }

  // TODO(crbug.com/450679848): Profile Picker doesn't make sense on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
  ProfilePicker::Show(
      ProfilePicker::Params::ForGlicManager(std::move(callback)));
#endif
}

void GlicProfileManager::DidSelectProfile(Profile* profile) {
  if (!GlicEnabling::IsEnabledForProfile(profile)) {
    return;
  }

  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile);

  if (!GlicEnabling::HasConsentedForProfile(profile) &&
      !GlicEnabling::IsTrustFirstOnboardingEnabled()) {
    // Open a browser and show the FRE in a new tab.
    chrome::ScopedTabbedBrowserDisplayer displayer(profile);
    service->OpenFreDialogInNewTab(displayer.browser(),
                                   mojom::InvocationSource::kProfilePicker);
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
  return last_active_glic_->IsWindowOrFreShowing();
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
    std::optional<base::MemoryPressureLevel> level) {
  g_forced_memory_pressure_level_ = level;
}

// static
void GlicProfileManager::ForceConnectionTypeForTesting(
    std::optional<network::mojom::ConnectionType> connection_type) {
  g_forced_connection_type_ = connection_type;
}

bool GlicProfileManager::IsUnderMemoryPressure() const {
  base::MemoryPressureLevel memory_pressure = base::MEMORY_PRESSURE_LEVEL_NONE;
  if (g_forced_memory_pressure_level_) {
    memory_pressure = *g_forced_memory_pressure_level_;
  } else if (const auto* memory_monitor = base::MemoryPressureMonitor::Get()) {
    memory_pressure = memory_monitor->GetCurrentPressureLevel(
        base::MemoryPressureMonitorTag::kGlicProfileManager);
  }
  return memory_pressure >= base::MEMORY_PRESSURE_LEVEL_MODERATE;
}

void GlicProfileManager::CanPreloadForProfile(Profile* profile,
                                              ShouldPreloadCallback callback) {
  auto produce_result = [&callback](GlicPrewarmingChecksResult result,
                                    base::Location from_here =
                                        base::Location::Current()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        from_here, base::BindOnce(std::move(callback), result));
  };
  if (!profile || IsProfileDirectoryMarkedForDeletion(profile->GetPath())) {
    return produce_result(GlicPrewarmingChecksResult::kProfileGone);
  }
  if (profile->ShutdownStarted()) {
    return produce_result(GlicPrewarmingChecksResult::kBrowserShuttingDown);
  }
  auto enablement = GlicEnabling::EnablementForProfile(profile);
  if (!enablement.IsProfileEligible()) {
    return produce_result(GlicPrewarmingChecksResult::kProfileNotEligible);
  }
  if (enablement.DisallowedByAdmin()) {
    return produce_result(
        GlicPrewarmingChecksResult::kProfileDisallowedByAdmin);
  }
  if (!enablement.IsEnabled()) {
    return produce_result(GlicPrewarmingChecksResult::kProfileNotEnabledOther);
  }
  if (last_loaded_glic_ && last_loaded_glic_->profile() == profile) {
    return produce_result(GlicPrewarmingChecksResult::kProfileIsLastLoaded);
  }
  if (last_active_glic_ && last_active_glic_->profile() == profile) {
    return produce_result(GlicPrewarmingChecksResult::kProfileIsLastActive);
  }
  if (!base::FeatureList::IsEnabled(features::kGlicWarmMultiple) &&
      IsShowing()) {
    return produce_result(GlicPrewarmingChecksResult::kBlockedByShownGlic);
  }
  if (IsUnderMemoryPressure()) {
    return produce_result(GlicPrewarmingChecksResult::kUnderMemoryPressure);
  }

  auto on_got_connection_type = [](ShouldPreloadCallback callback,
                                   network::mojom::ConnectionType type) {
    std::move(callback).Run(
        network::NetworkConnectionTracker::IsConnectionCellular(type)
            ? GlicPrewarmingChecksResult::kCellularConnection
            : GlicPrewarmingChecksResult::kSuccess);
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
            &connection_type,
            base::BindOnce(on_got_connection_type, std::move(callbacks.first)));
  }

  if (synchronously_got_connection_type) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(on_got_connection_type, std::move(callbacks.second),
                       connection_type));
  }
}

base::WeakPtr<GlicProfileManager> GlicProfileManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace glic
