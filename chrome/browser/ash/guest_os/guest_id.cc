// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_id.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace guest_os {
namespace {

static const base::NoDestructor<std::vector<std::string>> kPropertiesAllowList{{
    prefs::kContainerCreateOptions,
    prefs::kContainerOsVersionKey,
    prefs::kContainerOsPrettyNameKey,
    prefs::kContainerColorKey,
    prefs::kTerminalSupportedKey,
    prefs::kTerminalLabel,
    prefs::kTerminalPolicyDisabled,
    prefs::kContainerSharedVmDevicesKey,
    prefs::kBruschettaConfigId,
}};

}  // namespace

GuestId::GuestId(VmType vm_type,
                 std::string vm_name,
                 std::string container_name) noexcept
    : vm_type(vm_type),
      vm_name(std::move(vm_name)),
      container_name(std::move(container_name)) {}

GuestId::GuestId(std::string vm_name, std::string container_name) noexcept
    : vm_type(VmType::UNKNOWN),
      vm_name(std::move(vm_name)),
      container_name(std::move(container_name)) {}

GuestId::GuestId(const base::Value& value) noexcept {
  const base::Value::Dict* dict = value.GetIfDict();
  vm_type = VmTypeFromPref(value);
  const std::string* vm = nullptr;
  const std::string* container = nullptr;
  if (dict != nullptr) {
    vm = dict->FindString(prefs::kVmNameKey);
    container = dict->FindString(prefs::kContainerNameKey);
  }
  vm_name = vm ? *vm : "";
  container_name = container ? *container : "";
}

base::flat_map<std::string, std::string> GuestId::ToMap() const {
  base::flat_map<std::string, std::string> extras;
  extras[prefs::kVmNameKey] = vm_name;
  extras[prefs::kContainerNameKey] = container_name;
  return extras;
}

base::Value::Dict GuestId::ToDictValue() const {
  base::Value::Dict dict;
  dict.Set(prefs::kVmTypeKey, static_cast<int>(vm_type));
  dict.Set(prefs::kVmNameKey, vm_name);
  dict.Set(prefs::kContainerNameKey, container_name);
  return dict;
}

bool operator<(const GuestId& lhs, const GuestId& rhs) noexcept {
  int result = lhs.vm_name.compare(rhs.vm_name);
  if (result != 0) {
    return result < 0;
  }
  return lhs.container_name < rhs.container_name;
}

bool operator==(const GuestId& lhs, const GuestId& rhs) noexcept {
  return lhs.vm_name == rhs.vm_name && lhs.container_name == rhs.container_name;
}

std::ostream& operator<<(std::ostream& ostream, const GuestId& container_id) {
  return ostream << "(type:" << container_id.vm_type << ":"
                 << vm_tools::apps::VmType_Name(container_id.vm_type)
                 << " vm:\"" << container_id.vm_name << "\" container:\""
                 << container_id.container_name << "\")";
}

std::string GuestId::Serialize() const {
  return base::StringPrintf("%s:%s:%s", VmType_Name(this->vm_type),
                            this->vm_name, this->container_name);
}

std::optional<GuestId> Deserialize(std::string_view guest_id_string) {
  std::vector<std::string> string_tokens = base::SplitString(
      guest_id_string, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (string_tokens.size() != 3) {
    return {};
  }

  if (std::any_of(string_tokens.begin(), string_tokens.end(),
                  [](std::string v) { return v.empty(); })) {
    return {};
  }

  VmType vm_type;
  if (!VmType_Parse(string_tokens[0], &vm_type)) {
    return {};
  }

  return GuestId(vm_type, string_tokens[1], string_tokens[2]);
}

bool MatchContainerDict(const base::Value& dict, const GuestId& container_id) {
  const std::string* vm_name = dict.GetDict().FindString(prefs::kVmNameKey);
  const std::string* container_name =
      dict.GetDict().FindString(prefs::kContainerNameKey);
  return (vm_name && *vm_name == container_id.vm_name) &&
         (container_name && *container_name == container_id.container_name);
}

std::vector<GuestId> GetContainers(Profile* profile, VmType vm_type) {
  std::vector<GuestId> result;
  const base::Value::List& container_list =
      profile->GetPrefs()->GetList(prefs::kGuestOsContainers);
  for (const auto& container : container_list) {
    guest_os::GuestId id(container);
    if (id.vm_type == vm_type) {
      result.push_back(std::move(id));
    }
  }
  return result;
}

void AddContainerToPrefs(Profile* profile,
                         const GuestId& container_id,
                         base::Value::Dict properties) {
  ScopedListPrefUpdate updater(profile->GetPrefs(), prefs::kGuestOsContainers);
  if (base::ranges::any_of(*updater, [&container_id](const auto& dict) {
        return MatchContainerDict(dict, container_id);
      })) {
    return;
  }

  base::Value::Dict new_container = container_id.ToDictValue();
  for (auto [key, value] : properties) {
    if (base::Contains(*kPropertiesAllowList, key)) {
      new_container.Set(key, std::move(value));
    }
  }
  updater->Append(std::move(new_container));
}

void RemoveContainerFromPrefs(Profile* profile, const GuestId& container_id) {
  auto* pref_service = profile->GetPrefs();
  ScopedListPrefUpdate updater(pref_service, prefs::kGuestOsContainers);
  base::Value::List& update_list = updater.Get();
  auto it = base::ranges::find_if(update_list, [&](const auto& dict) {
    return MatchContainerDict(dict, container_id);
  });
  if (it != update_list.end()) {
    update_list.erase(it);
  }
}

void RemoveVmFromPrefs(Profile* profile, VmType vm_type) {
  auto* pref_service = profile->GetPrefs();
  ScopedListPrefUpdate updater(pref_service, prefs::kGuestOsContainers);
  base::Value::List& update_list = updater.Get();
  auto it = base::ranges::find(update_list, vm_type, &VmTypeFromPref);
  if (it != update_list.end()) {
    update_list.erase(it);
  }
}

const base::Value* GetContainerPrefValue(Profile* profile,
                                         const GuestId& container_id,
                                         const std::string& key) {
  const base::Value::List& containers =
      profile->GetPrefs()->GetList(prefs::kGuestOsContainers);
  for (const auto& dict : containers) {
    if (MatchContainerDict(dict, container_id)) {
      return dict.GetDict().Find(key);
    }
  }
  return nullptr;
}

void UpdateContainerPref(Profile* profile,
                         const GuestId& container_id,
                         const std::string& key,
                         base::Value value) {
  ScopedListPrefUpdate updater(profile->GetPrefs(), prefs::kGuestOsContainers);
  auto it = base::ranges::find_if(*updater, [&](const auto& dict) {
    return MatchContainerDict(dict, container_id);
  });
  if (it != updater->end()) {
    if (base::Contains(*kPropertiesAllowList, key)) {
      it->GetDict().Set(key, std::move(value));
    } else {
      LOG(ERROR) << "Ignoring disallowed property: " << key;
    }
  }
}

void MergeContainerPref(Profile* profile,
                        const GuestId& container_id,
                        const std::string& key,
                        base::Value::Dict dict) {
  ScopedListPrefUpdate updater(profile->GetPrefs(), prefs::kGuestOsContainers);
  auto it = base::ranges::find_if(*updater, [&](const auto& dict) {
    return MatchContainerDict(dict, container_id);
  });
  if (it != updater->end()) {
    if (base::Contains(*kPropertiesAllowList, key)) {
      base::Value::Dict* old_container_dict = it->GetIfDict();
      if (old_container_dict) {
        base::Value::Dict wrapped;
        wrapped.Set(key, std::move(dict));
        old_container_dict->Merge(std::move(wrapped));
      } else {
        LOG(ERROR) << "Expected a dict for " << container_id;
      }
    } else {
      LOG(ERROR) << "Ignoring disallowed property: " << key;
    }
  }
}

VmType VmTypeFromPref(const base::Value& pref) {
  if (!pref.is_dict()) {
    return VmType::UNKNOWN;
  }

  // Default is TERMINA(0) if field not present since this field was introduced
  // when only TERMINA was using prefs..
  auto type = pref.GetDict().FindInt(guest_os::prefs::kVmTypeKey);
  if (!type.has_value()) {
    LOG(WARNING) << "No VM type in pref, defaulting to termina";
    return VmType::TERMINA;
  }
  if (*type < vm_tools::apps::VmType_MIN ||
      *type > vm_tools::apps::VmType_MAX) {
    return VmType::UNKNOWN;
  }
  return static_cast<guest_os::VmType>(*type);
}

}  // namespace guest_os
