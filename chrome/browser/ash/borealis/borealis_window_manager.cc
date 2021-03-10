// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_window_manager.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/chromeos/crostini/crostini_shelf_utils.h"
#include "components/exo/shell_surface_util.h"

namespace {

// Borealis windows are created with app/startup ids beginning with this.
const char kBorealisWindowPrefix[] = "org.chromium.borealis.";

// Anonymous apps do not have a CrOS-standard app_id (i.e. one registered with
// the GuestOsRegistryService), so to identify them we prepend this.
const char kBorealisAnonymousPrefix[] = "borealis_anon:";

// Returns an ID for this window (which is the app_id or startup_id, depending
// on which are set. The ID string is owned by the window.
const std::string* GetWindowId(aura::Window* window) {
  const std::string* id = exo::GetShellApplicationId(window);
  if (id)
    return id;
  return exo::GetShellStartupId(window);
}

std::string WindowToAppId(Profile* profile, aura::Window* window) {
  // TODO(b/173977876): When we have better ways of associating apps with
  // windows we will implement them. Until then, the mapping is identical to
  // Crostini's so just spoof the relevant information and use theirs.
  std::string pretend_crostini_id(*GetWindowId(window));
  base::ReplaceFirstSubstringAfterOffset(
      &pretend_crostini_id, 0, kBorealisWindowPrefix, "org.chromium.termina.");
  std::string crostini_equivalent_id =
      crostini::GetCrostiniShelfAppId(profile, &pretend_crostini_id, nullptr);

  // If crostini thinks this app is registered, then it actually is registered
  // for borealis.
  if (!crostini::IsUnmatchedCrostiniShelfAppId(crostini_equivalent_id))
    return crostini_equivalent_id;

  return kBorealisAnonymousPrefix + *GetWindowId(window);
}

// Returns a name for the app with the given |anon_id|.
std::string AnonymousIdentifierToName(const std::string& anon_id) {
  return anon_id.substr(anon_id.find(kBorealisWindowPrefix) +
                        sizeof(kBorealisWindowPrefix) - 1);
}

bool IsAnonymousAppId(const std::string& app_id) {
  return base::StartsWith(app_id, kBorealisAnonymousPrefix,
                          base::CompareCase::SENSITIVE);
}

}  // namespace

namespace borealis {

// static
bool BorealisWindowManager::IsBorealisWindow(aura::Window* window) {
  const std::string* id = GetWindowId(window);
  if (!id)
    return false;
  return IsBorealisWindowId(*id);
}

// static
bool BorealisWindowManager::IsBorealisWindowId(const std::string& window_id) {
  return base::StartsWith(window_id, kBorealisWindowPrefix);
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
  if (!IsBorealisWindow(window))
    return {};

  // We delay the observation until the first time we actually see a borealis
  // window, which prevents unnecessary messages being sent and breaks an
  // init-cycle.
  if (!instance_registry_observation_.IsObserving()) {
    instance_registry_observation_.Observe(
        &apps::AppServiceProxyFactory::GetForProfile(profile_)
             ->InstanceRegistry());
  }

  return WindowToAppId(profile_, window);
}

void BorealisWindowManager::OnInstanceUpdate(
    const apps::InstanceUpdate& update) {
  if (!IsBorealisWindow(update.Window()))
    return;
  if (update.IsCreation()) {
    HandleWindowCreation(update.Window(), update.AppId());
  } else if (update.IsDestruction()) {
    HandleWindowDestruction(update.Window(), update.AppId());
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
    observer.OnAppFinished(app_id);

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
      std::string anon_name = AnonymousIdentifierToName(app_id);
      for (auto& observer : anon_observers_)
        observer.OnAnonymousAppAdded(app_id, anon_name);
    }
  }
  // If this window was not already in the set, notify our observers about it.
  if (ids_to_windows_[app_id].emplace(window).second) {
    for (auto& observer : lifetime_observers_)
      observer.OnWindowStarted(app_id, window);
  }
}

}  // namespace borealis
