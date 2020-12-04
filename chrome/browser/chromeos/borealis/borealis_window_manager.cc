// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_window_manager.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/borealis/borealis_util.h"
#include "chrome/browser/chromeos/crostini/crostini_shelf_utils.h"
#include "components/exo/shell_surface_util.h"
#include "ui/aura/window.h"

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

std::string WindowToAnonymousAppId(aura::Window* window) {
  return kBorealisAnonymousPrefix + *GetWindowId(window);
}

// Returns a name for the app with the given |anon_id|.
std::string AnonymousIdentifierToName(const std::string& anon_id) {
  return anon_id.substr(anon_id.find(kBorealisWindowPrefix) +
                        sizeof(kBorealisWindowPrefix) - 1);
}

}  // namespace

namespace borealis {

// static
bool BorealisWindowManager::IsBorealisWindow(aura::Window* window) {
  const std::string* id = GetWindowId(window);
  if (!id)
    return false;
  return base::StartsWith(*id, kBorealisWindowPrefix);
}

BorealisWindowManager::BorealisWindowManager(Profile* profile)
    : profile_(profile) {}

BorealisWindowManager::~BorealisWindowManager() {
  for (auto& id_to_windows : anon_ids_to_windows_) {
    for (aura::Window* window : id_to_windows.second) {
      window->RemoveObserver(this);
    }
    for (auto& observer : observers_) {
      observer.OnAnonymousAppRemoved(id_to_windows.first);
    }
  }
  for (auto& observer : observers_) {
    observer.OnWindowManagerDeleted(this);
  }
  DCHECK(!observers_.might_have_observers());
}

void BorealisWindowManager::AddObserver(AnonymousAppObserver* observer) {
  observers_.AddObserver(observer);
}

void BorealisWindowManager::RemoveObserver(AnonymousAppObserver* observer) {
  observers_.RemoveObserver(observer);
}

std::string BorealisWindowManager::GetShelfAppId(aura::Window* window) {
  if (!IsBorealisWindow(window))
    return {};
  // TODO(b/173977876): When we have better ways of associating apps with
  // windows we will implement them. Until then, the mapping is identical to
  // Crostini's so just spoof the relevant information and use theirs.
  std::string pretend_crostini_id(*GetWindowId(window));
  base::ReplaceFirstSubstringAfterOffset(
      &pretend_crostini_id, 0, kBorealisWindowPrefix, "org.chromium.termina.");
  std::string crostini_equivalent_id =
      crostini::GetCrostiniShelfAppId(profile_, &pretend_crostini_id, nullptr);

  // If crostini thinks this app is registered, then it actually is registered
  // for borealis.
  if (!crostini::IsUnmatchedCrostiniShelfAppId(crostini_equivalent_id))
    return crostini_equivalent_id;

  // The app has no registration, it is anonymous.
  std::string anon_id = WindowToAnonymousAppId(window);
  if (!anon_ids_to_windows_.contains(anon_id)) {
    std::string anon_name = AnonymousIdentifierToName(anon_id);
    for (auto& observer : observers_)
      observer.OnAnonymousAppAdded(anon_id, anon_name);
  }
  // Add the window to the tracking set, and if it wasn't already there, add an
  // observer.
  if (anon_ids_to_windows_[anon_id].emplace(window).second) {
    window->AddObserver(this);
  }
  return anon_id;
}

void BorealisWindowManager::OnWindowDestroying(aura::Window* window) {
  std::string anon_id = WindowToAnonymousAppId(window);
  base::flat_map<std::string, base::flat_set<aura::Window*>>::iterator iter =
      anon_ids_to_windows_.find(anon_id);

  DCHECK(iter != anon_ids_to_windows_.end());
  DCHECK(iter->second.contains(window));

  iter->second.erase(window);
  if (!iter->second.empty())
    return;

  for (auto& observer : observers_)
    observer.OnAnonymousAppRemoved(anon_id);
  anon_ids_to_windows_.erase(iter);
}

}  // namespace borealis
