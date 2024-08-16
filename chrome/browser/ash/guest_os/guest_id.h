// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_ID_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_ID_H_

#include <ostream>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/values.h"
#include "chrome/browser/ash/guest_os/public/types.h"

class Profile;

namespace guest_os {

// A unique identifier for our containers.
struct GuestId {
  GuestId(VmType vm_type,
          std::string vm_name,
          std::string container_name) noexcept;
  GuestId(std::string vm_name, std::string container_name) noexcept;
  explicit GuestId(const base::Value&) noexcept;

  base::flat_map<std::string, std::string> ToMap() const;
  base::Value::Dict ToDictValue() const;
  std::string Serialize() const;

  VmType vm_type;
  std::string vm_name;
  std::string container_name;
};

bool operator<(const GuestId& lhs, const GuestId& rhs) noexcept;
bool operator==(const GuestId& lhs, const GuestId& rhs) noexcept;
inline bool operator!=(const GuestId& lhs, const GuestId& rhs) noexcept {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& ostream, const GuestId& container_id);

std::optional<GuestId> Deserialize(std::string_view guest_id_string);

// Returns a list of all containers in prefs.
std::vector<GuestId> GetContainers(Profile* profile, VmType vm_type);

// Returns true if the container_id's vm_name and container_name matches entries
// in the dict.
bool MatchContainerDict(const base::Value& dict, const GuestId& container_id);

// Add a new container to the kGuestOsContainers pref
void AddContainerToPrefs(Profile* profile,
                         const GuestId& container_id,
                         base::Value::Dict properties);

// Remove a deleted container from the kGuestOsContainers pref.
void RemoveContainerFromPrefs(Profile* profile, const GuestId& container_id);

// Remove all containers for the specified |vm_type| from the kGuestOsContainers
// pref.
void RemoveVmFromPrefs(Profile* profile, VmType vm_type);

// Returns a pref value stored for a specific container.
const base::Value* GetContainerPrefValue(Profile* profile,
                                         const GuestId& container_id,
                                         const std::string& key);

// Sets a pref value for a specific container.
void UpdateContainerPref(Profile* profile,
                         const GuestId& container_id,
                         const std::string& key,
                         base::Value value);

// Merges |dict| into the existing dict pref value for |key| a specific
// container. Sets |dict| as the value for |key| otherwise.
void MergeContainerPref(Profile* profile,
                        const GuestId& container_id,
                        const std::string& key,
                        base::Value::Dict dict);

// Get "vm_type" int from pref and convert to VmType using TERMINA(0) as default
// if field is not present.
VmType VmTypeFromPref(const base::Value& pref);

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_ID_H_
