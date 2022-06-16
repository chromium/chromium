// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_id.h"

#include "chrome/browser/ash/crostini/crostini_pref_names.h"

namespace guest_os {

GuestId::GuestId(std::string vm_name, std::string container_name) noexcept
    : vm_name(std::move(vm_name)), container_name(std::move(container_name)) {}

GuestId::GuestId(const base::Value& value) noexcept {
  const base::Value::Dict* dict = value.GetIfDict();
  const std::string* vm = nullptr;
  const std::string* container = nullptr;
  if (dict != nullptr) {
    vm = dict->FindString(crostini::prefs::kVmKey);
    container = dict->FindString(crostini::prefs::kContainerKey);
  }
  vm_name = vm ? *vm : "";
  container_name = container ? *container : "";
}

base::flat_map<std::string, std::string> GuestId::ToMap() const {
  base::flat_map<std::string, std::string> extras;
  extras[crostini::prefs::kVmKey] = vm_name;
  extras[crostini::prefs::kContainerKey] = container_name;
  return extras;
}

base::Value::Dict GuestId::ToDictValue() const {
  base::Value::Dict dict;
  dict.Set(crostini::prefs::kVmKey, vm_name);
  dict.Set(crostini::prefs::kContainerKey, container_name);
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

}  // namespace guest_os
