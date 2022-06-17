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
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace guest_os {
namespace {

bool MatchContainerDict(const base::Value& dict, const GuestId& container_id) {
  const std::string* vm_name = dict.FindStringKey(prefs::kVmKey);
  const std::string* container_name = dict.FindStringKey(prefs::kContainerKey);
  return (vm_name && *vm_name == container_id.vm_name) &&
         (container_name && *container_name == container_id.container_name);
}

static const base::NoDestructor<std::vector<std::string>> kPropertiesAllowList{{
    prefs::kContainerOsVersionKey,
    prefs::kContainerOsPrettyNameKey,
    prefs::kContainerColorKey,
}};

}  // namespace

GuestId::GuestId(std::string vm_name, std::string container_name) noexcept
    : vm_name(std::move(vm_name)), container_name(std::move(container_name)) {}

GuestId::GuestId(const base::Value& value) noexcept {
  const base::Value::Dict* dict = value.GetIfDict();
  const std::string* vm = nullptr;
  const std::string* container = nullptr;
  if (dict != nullptr) {
    vm = dict->FindString(prefs::kVmKey);
    container = dict->FindString(prefs::kContainerKey);
  }
  vm_name = vm ? *vm : "";
  container_name = container ? *container : "";
}

base::flat_map<std::string, std::string> GuestId::ToMap() const {
  base::flat_map<std::string, std::string> extras;
  extras[prefs::kVmKey] = vm_name;
  extras[prefs::kContainerKey] = container_name;
  return extras;
}

base::Value::Dict GuestId::ToDictValue() const {
  base::Value::Dict dict;
  dict.Set(prefs::kVmKey, vm_name);
  dict.Set(prefs::kContainerKey, container_name);
  return dict;
}

bool operator<(const GuestId& lhs, const GuestId& rhs) noexcept {
  const auto result = lhs.vm_name.compare(rhs.vm_name);
  return result < 0 || (result == 0 && lhs.container_name < rhs.container_name);
}

bool operator==(const GuestId& lhs, const GuestId& rhs) noexcept {
  return lhs.vm_name == rhs.vm_name && lhs.container_name == rhs.container_name;
}

std::ostream& operator<<(std::ostream& ostream, const GuestId& container_id) {
  return ostream << "(vm: \"" << container_id.vm_name << "\" container: \""
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

std::vector<GuestId> GetContainers(Profile* profile) {
  std::vector<GuestId> result;
  const base::Value::List& container_list =
      profile->GetPrefs()->GetList(prefs::kGuestOsContainers)->GetList();
  for (const auto& container : container_list) {
    guest_os::GuestId id(container);
    if (!id.vm_name.empty() && !id.container_name.empty()) {
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
  new_container.SetKey(prefs::kVmKey, base::Value(container_id.vm_name));
  new_container.SetKey(prefs::kContainerKey,
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

void RemoveVmFromPrefs(Profile* profile, const std::string& vm_name) {
  auto* pref_service = profile->GetPrefs();
  ListPrefUpdate updater(pref_service, prefs::kGuestOsContainers);
  updater->EraseListIter(std::find_if(
      updater->GetListDeprecated().begin(), updater->GetListDeprecated().end(),
      [&](const auto& dict) {
        const std::string* dict_vm_name = dict.FindStringKey(prefs::kVmKey);
        return dict_vm_name && *dict_vm_name == vm_name;
      }));
}

const base::Value* GetContainerPrefValue(Profile* profile,
                                         const GuestId& container_id,
                                         const std::string& key) {
  const base::Value* containers =
      profile->GetPrefs()->GetList(prefs::kGuestOsContainers);
  if (!containers) {
    return nullptr;
  }
  for (const auto& dict : containers->GetListDeprecated()) {
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

}  // namespace guest_os
