// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_ID_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_ID_H_

#include <ostream>
#include <string>

#include "base/containers/flat_map.h"
#include "base/values.h"
#include "chrome/browser/ash/guest_os/public/types.h"

class PrefService;
class Profile;

namespace guest_os {

// A unique identifier for our guests.
struct GuestId {
  GuestId(std::string vm_name, std::string container_name) noexcept;
  explicit GuestId(const base::Value&) noexcept;

  base::flat_map<std::string, std::string> ToMap() const;
  base::Value::Dict ToDictValue() const;

  std::string vm_name;
  std::string container_name;
};

bool operator<(const GuestId& lhs, const GuestId& rhs) noexcept;
bool operator==(const GuestId& lhs, const GuestId& rhs) noexcept;
inline bool operator!=(const GuestId& lhs, const GuestId& rhs) noexcept {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& ostream, const GuestId& container_id);

// Returns a list of all containers in prefs.
std::vector<GuestId> GetContainers(Profile* profile);

// Remove duplicate containers in the existing kGuestOsContainers pref.
void RemoveDuplicateContainerEntries(PrefService* prefs);

// Add a new container to the kGuestOsContainers pref
void AddContainerToPrefs(Profile* profile,
                         const GuestId& container_id,
                         base::Value::Dict properties);

// Remove a deleted container from the kGuestOsContainers pref.
void RemoveContainerFromPrefs(Profile* profile, const GuestId& container_id);

// Remove a deleted container from the kGuestOsContainers pref.
void RemoveVmFromPrefs(Profile* profile, const std::string& vm_name);

// Returns a pref value stored for a specific container.
const base::Value* GetContainerPrefValue(Profile* profile,
                                         const GuestId& container_id,
                                         const std::string& key);

// Sets a pref value for a specific container.
void UpdateContainerPref(Profile* profile,
                         const GuestId& container_id,
                         const std::string& key,
                         base::Value value);

// Get "vm_type" int from pref and convert to VmType using TERMINA(0) as default
// if field is not present.
VmType VmTypeFromPref(const base::Value& pref);

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_ID_H_
