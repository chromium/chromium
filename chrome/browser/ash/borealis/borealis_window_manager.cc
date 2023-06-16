// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_window_manager.h"

#include <string>

#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/base64.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_shelf_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/exo/shell_surface_util.h"
#include "components/prefs/pref_service.h"

namespace borealis {

const char kBorealisWindowPrefix[] = "org.chromium.guest_os.borealis.";
const char kFullscreenClientShellId[] =
    "org.chromium.guest_os.borealis.wmclass.steam";
const char kBorealisClientSuffix[] = "wmclass.Steam";
const char kBorealisAnonymousPrefix[] = "borealis_anon:";

namespace {
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kShelfAppIdKey, nullptr)

// Returns an ID for this window (which is the app_id or startup_id, depending
// on which are set. The ID string is owned by the window.
const std::string* GetWindowId(const aura::Window* window) {
  const std::string* id = exo::GetShellApplicationId(window);
  if (id)
    return id;
  return exo::GetShellStartupId(window);
}

// Return the app ID of an installed app with the given Borealis ID.
//
// Relies on the Exec line in the desktop entry (.desktop file within the VM)
// having the expected format.
std::string BorealisIdToAppId(Profile* profile, unsigned borealis_id) {
  for (const auto& item :
       guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile)
           ->GetRegisteredApps(guest_os::VmType::BOREALIS)) {
    absl::optional<int> app_id = GetBorealisAppId(item.second.Exec());
    if (app_id && app_id.value() == static_cast<int>(borealis_id)) {
      return item.first;
    }
  }
  return {};
}

std::string WindowToAppId(Profile* profile, const aura::Window* window) {
  // The Borealis app ID is the most reliable method, if known.
  absl::optional<int> borealis_id = GetBorealisAppId(window);
  if (borealis_id.has_value()) {
    std::string app_id = BorealisIdToAppId(profile, borealis_id.value());
    if (!app_id.empty()) {
      return app_id;
    } else if (borealis_id.value() == 769) {
      // TODO(b/287506770): Figure out why valve started doing this, come up
      // with a more resilient solution.
      return kClientAppId;
    }
  }

  // Fall back to GuestOS's logic for associating windows with apps.
  // GetGuestOsShelfAppId will handle both registered and anonymous borealis app
  // windows correctly. For registered apps, it will return a matching shelf app
  // ID. For unregistered apps, it will return an app_id prefixed with
  // "borealis_anon:".
  // TODO(cpelling): Log a warning here once all Steam startup windows and
  // games are correctly registered.
  return guest_os::GetGuestOsShelfAppId(profile, GetWindowId(window), nullptr);
}

}  // namespace

// static
bool BorealisWindowManager::IsBorealisWindow(const aura::Window* window) {
  const std::string* id = GetWindowId(window);
  if (!id)
    return false;
  return IsBorealisWindowId(*id);
}

// static
bool BorealisWindowManager::IsBorealisWindowId(const std::string& window_id) {
  return base::StartsWith(window_id, borealis::kBorealisWindowPrefix);
}

// static
bool BorealisWindowManager::ShouldNewWindowBeMinimized(
    const std::string& window_id) {
  // Only borealis client windows should be minimized.
  if (!base::EndsWith(window_id, borealis::kBorealisClientSuffix,
                      base::CompareCase::SENSITIVE)) {
    return false;
  }

  // We only need to create windows as minimized when the active window is a
  // borealis window in fullscreen.
  aura::Window* active_window = ash::window_util::GetActiveWindow();
  if (!active_window || !IsBorealisWindow(active_window))
    return false;

  const std::string* active_window_id = GetWindowId(active_window);
  if (!active_window_id)
    return false;

  auto* window_state = ash::WindowState::Get(active_window);
  if (!window_state || !window_state->IsFullscreen())
    return false;

  // If the fullscreen window is the borealis client, then we allow windows to
  // take focus.
  if (*active_window_id == borealis::kFullscreenClientShellId) {
    return false;
  }

  return true;
}

// static
bool BorealisWindowManager::IsAnonymousAppId(const std::string& app_id) {
  return base::StartsWith(app_id, kBorealisAnonymousPrefix,
                          base::CompareCase::SENSITIVE);
}

BorealisWindowManager::BorealisWindowManager(Profile* profile)
    : profile_(profile), instance_registry_observation_(this) {}

BorealisWindowManager::~BorealisWindowManager() {
  for (auto& observer : anon_observers_) {
    observer.OnWindowManagerDeleted(this);
  }
  for (auto& observer : lifetime_observers_) {
    observer.OnWindowManagerDeleted(this);
  }
  DCHECK(anon_observers_.empty());
  DCHECK(lifetime_observers_.empty());
}

void BorealisWindowManager::AddObserver(AnonymousAppObserver* observer) {
  anon_observers_.AddObserver(observer);
}

void BorealisWindowManager::RemoveObserver(AnonymousAppObserver* observer) {
  anon_observers_.RemoveObserver(observer);
}

void BorealisWindowManager::AddObserver(AppWindowLifetimeObserver* observer) {
  lifetime_observers_.AddObserver(observer);
}

void BorealisWindowManager::RemoveObserver(
    AppWindowLifetimeObserver* observer) {
  lifetime_observers_.RemoveObserver(observer);
}

std::string BorealisWindowManager::GetShelfAppId(aura::Window* window) {
  if (!IsBorealisWindow(window)) {
    return {};
  }

  // We delay the observation until the first time we actually see a borealis
  // window, which prevents unnecessary messages being sent and breaks an
  // init-cycle.
  if (!instance_registry_observation_.IsObserving()) {
    instance_registry_observation_.Observe(
        &apps::AppServiceProxyFactory::GetForProfile(profile_)
             ->InstanceRegistry());
  }

  if (!window->GetProperty(kShelfAppIdKey))
    window->SetProperty(kShelfAppIdKey, WindowToAppId(profile_, window));
  return *window->GetProperty(kShelfAppIdKey);
}

void BorealisWindowManager::OnInstanceUpdate(
    const apps::InstanceUpdate& update) {
  aura::Window* window = update.Window();
  if (!IsBorealisWindow(window))
    return;
  if (update.IsCreation()) {
    HandleWindowCreation(window, update.AppId());
  } else if (update.IsDestruction()) {
    HandleWindowDestruction(window, update.AppId());
  }
}

void BorealisWindowManager::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* cache) {
  DCHECK(instance_registry_observation_.IsObservingSource(cache));
  instance_registry_observation_.Reset();
}

void BorealisWindowManager::HandleWindowDestruction(aura::Window* window,
                                                    const std::string& app_id) {
  for (auto& observer : lifetime_observers_) {
    observer.OnWindowFinished(app_id, window);
  }

  base::flat_map<std::string, base::flat_set<aura::Window*>>::iterator iter =
      ids_to_windows_.find(app_id);
  DCHECK(iter != ids_to_windows_.end());
  DCHECK(iter->second.contains(window));
  iter->second.erase(window);
  if (!iter->second.empty())
    return;

  if (IsAnonymousAppId(app_id)) {
    for (auto& observer : anon_observers_)
      observer.OnAnonymousAppRemoved(app_id);
  }
  for (auto& observer : lifetime_observers_)
    observer.OnAppFinished(app_id, window);

  ids_to_windows_.erase(iter);
  if (!ids_to_windows_.empty())
    return;
  for (auto& observer : lifetime_observers_)
    observer.OnSessionFinished();
}

void BorealisWindowManager::HandleWindowCreation(aura::Window* window,
                                                 const std::string& app_id) {
  // If this is the first window, the session has started.
  if (ids_to_windows_.empty()) {
    for (auto& observer : lifetime_observers_)
      observer.OnSessionStarted();
  }
  // If this is the given app_id's first window, the app has started
  if (ids_to_windows_[app_id].empty()) {
    for (auto& observer : lifetime_observers_)
      observer.OnAppStarted(app_id);
    if (IsAnonymousAppId(app_id)) {
      for (auto& observer : anon_observers_)
        observer.OnAnonymousAppAdded(app_id,
                                     base::UTF16ToUTF8(window->GetTitle()));
    }
  }
  // If this window was not already in the set, notify our observers about it.
  if (ids_to_windows_[app_id].emplace(window).second) {
    for (auto& observer : lifetime_observers_)
      observer.OnWindowStarted(app_id, window);
  }
}

}  // namespace borealis
