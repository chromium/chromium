// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider_registry.h"

#include <climits>
#include <memory>
#include <vector>
#include "base/observer_list.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider.h"

namespace guest_os {

GuestOsMountProviderRegistry::GuestOsMountProviderRegistry() = default;
GuestOsMountProviderRegistry::~GuestOsMountProviderRegistry() = default;

std::vector<GuestOsMountProviderRegistry::Id>
GuestOsMountProviderRegistry::List() {
  std::vector<Id> ret = std::vector<Id>();
  ret.reserve(providers_.size());
  for (const auto& pair : providers_) {
    ret.push_back(pair.first);
  }
  return ret;
}

GuestOsMountProvider* GuestOsMountProviderRegistry::Get(Id id) const {
  auto pos = providers_.find(id);
  if (pos == providers_.end()) {
    return nullptr;
  }
  return pos->second.get();
}

GuestOsMountProviderRegistry::Id GuestOsMountProviderRegistry::Register(
    std::unique_ptr<GuestOsMountProvider> provider) {
  // We use the range 0->INT_MAX because these IDs can get serialised into
  // base::Value, and that's the range they support.
  CHECK(next_id_ < INT_MAX);
  Id id = next_id_++;
  providers_[id] = std::move(provider);
  for (auto& observer : observers_) {
    observer.OnRegistered(id, providers_[id].get());
  }
  return id;
}

std::unique_ptr<GuestOsMountProvider> GuestOsMountProviderRegistry::Unregister(
    Id id) {
  auto pos = providers_.find(id);
  // No one should be unregistering random providers, so it's an error to try
  // and unregister one which doesn't exist rather than a no-op.
  CHECK(pos != providers_.end());
  auto ret = std::move(pos->second);
  providers_.erase(pos);
  for (auto& observer : observers_) {
    observer.OnUnregistered(id);
  }
  return ret;
}

void GuestOsMountProviderRegistry::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GuestOsMountProviderRegistry::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace guest_os
