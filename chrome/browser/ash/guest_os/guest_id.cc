// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_id.h"

#include <memory>
#include <vector>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace guest_os {
namespace {

bool MatchContainerDict(const base::Value& dict, const GuestId& container_id) {
  const std::string* vm_name = dict.FindStringKey(prefs::kVmNameKey);
  const std::string* container_name =
      dict.FindStringKey(prefs::kContainerNameKey);
  return (vm_name && *vm_name == container_id.vm_name) &&
         (container_name && *container_name == container_id.container_name);
}

static const base::NoDestructor<std::vector<std::string>> kPropertiesAllowList{{
    prefs::kContainerOsVersionKey,
    prefs::kContainerOsPrettyNameKey,
    prefs::kContainerColorKey,
}};

}  // namespace

GuestId::GuestId(VmType vm_type,
                 std::string vm_name,
                 std::string container_name) noexcept
    : vm_type(vm_type),
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

void RemoveDuplicateContainerEntries(PrefService* prefs) {
  ListPrefUpdate updater(prefs, prefs::kGuestOsContainers);

  std::set<GuestId> seen_containers;
  auto& containers = updater->GetList();
  for (auto it = containers.begin(); it != containers.end();) {
    GuestId id(*it);
    if (seen_containers.find(id) == seen_containers.end()) {
      seen_containers.insert(id);
      it++;
    } else {
      it = containers.erase(it);
    }
  }
}

std::vector<GuestId> GetContainers(Profile* profile, VmType vm_type) {
  std::vector<GuestId> result;
  const base::Value::List& container_list =
      profile->GetPrefs()->GetList(prefs::kGuestOsContainers)->GetList();
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
  ListPrefUpdate updater(profile->GetPrefs(), prefs::kGuestOsContainers);
  auto it = std::find_if(
      updater->GetListDeprecated().begin(), updater->GetListDeprecated().end(),
      [&](const auto& dict) { return MatchContainerDict(dict, container_id); });
  if (it != updater->GetListDeprecated().end()) {
    return;
  }

  base::Value new_container(base::Value::Type::DICTIONARY);
  new_container.SetKey(prefs::kVmNameKey, base::Value(container_id.vm_name));
  new_container.SetKey(prefs::kContainerNameKey,
                       base::Value(container_id.container_name));
  for (const auto item : properties) {
    if (base::Contains(*kPropertiesAllowList, item.first)) {
      new_container.SetKey(std::move(item.first), std::move(item.second));
    }
  }
  updater->Append(std::move(new_container));
}

void RemoveContainerFromPrefs(Profile* profile, const GuestId& container_id) {
  auto* pref_service = profile->GetPrefs();
  ListPrefUpdate updater(pref_service, prefs::kGuestOsContainers);
  updater->EraseListIter(
      std::find_if(updater->GetListDeprecated().begin(),
                   updater->GetListDeprecated().end(), [&](const auto& dict) {
                     return MatchContainerDict(dict, container_id);
                   }));
}

void RemoveVmFromPrefs(Profile* profile, VmType vm_type) {
  auto* pref_service = profile->GetPrefs();
  ListPrefUpdate updater(pref_service, prefs::kGuestOsContainers);
  updater->EraseListIter(std::find_if(
      updater->GetListDeprecated().begin(), updater->GetListDeprecated().end(),
      [&](const auto& dict) { return VmTypeFromPref(dict) == vm_type; }));
}

const base::Value* GetContainerPrefValue(Profile* profile,
                                         const GuestId& container_id,
                                         const std::string& key) {
  const base::Value::List& containers =
      profile->GetPrefs()->GetValueList(prefs::kGuestOsContainers);
  for (const auto& dict : containers) {
    if (MatchContainerDict(dict, container_id))
      return dict.FindKey(key);
  }
  return nullptr;
}

void UpdateContainerPref(Profile* profile,
                         const GuestId& container_id,
                         const std::string& key,
                         base::Value value) {
  ListPrefUpdate updater(profile->GetPrefs(), prefs::kGuestOsContainers);
  auto it = std::find_if(
      updater->GetListDeprecated().begin(), updater->GetListDeprecated().end(),
      [&](const auto& dict) { return MatchContainerDict(dict, container_id); });
  if (it != updater->GetListDeprecated().end()) {
    if (base::Contains(*kPropertiesAllowList, key)) {
      it->SetKey(key, std::move(value));
    }
  }
}

VmType VmTypeFromPref(const base::Value& pref) {
  if (!pref.is_dict()) {
    return VmType::UNKNOWN;
  }

  // Default is TERMINA(0) if field not present since this field was introduced
  // when only TERMINA was using prefs..
  auto type = pref.FindIntKey(guest_os::prefs::kVmTypeKey);
  if (!type.has_value()) {
    return VmType::TERMINA;
  }
  if (*type < vm_tools::apps::VmType_MIN ||
      *type > vm_tools::apps::VmType_MAX) {
    return VmType::UNKNOWN;
  }
  return static_cast<guest_os::VmType>(*type);
}

}  // namespace guest_os
