// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_MOUNT_PROVIDER_REGISTRY_H_
#define CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_MOUNT_PROVIDER_REGISTRY_H_

#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace guest_os {

class GuestOsMountProvider;

class GuestOsMountProviderRegistry {
 public:
  // Id used to reference a specific provider within the registry. Note: These
  // IDs are not stable across restarts and there's no correlation between a
  // provider's ID and its VM/Guest, or between different providers.
  // Use int and start at 0 so we can serialise it to json without any
  // conversions.
  using Id = int;

  // Watches for new providers getting registered or unregistered from this
  // registry.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnRegistered(Id id, GuestOsMountProvider* provider) = 0;
    virtual void OnUnregistered(Id id) = 0;
  };

  // Gets a list of `Id`s for all provider's in the registry.
  std::vector<Id> List();

  GuestOsMountProviderRegistry();
  ~GuestOsMountProviderRegistry();
  GuestOsMountProviderRegistry(const GuestOsMountProviderRegistry&) = delete;
  GuestOsMountProviderRegistry& operator=(const GuestOsMountProviderRegistry&) =
      delete;

  // Returns the provider with the specified `Id`. Returns nullptr if the
  // provider doesn't exist.
  GuestOsMountProvider* Get(Id id) const;

  // Registers a new provider with the registry. The registry takes ownership of
  // the provider, holding on to it until it's unregistered. Returns the id of
  // the newly-registered provider.
  Id Register(std::unique_ptr<GuestOsMountProvider> provider);

  // Removes a provider from the registry, returning the provider. The specified
  // provider must be in the registry.
  std::unique_ptr<GuestOsMountProvider> Unregister(Id provider);

  // Adds an observer for changes to the registry.
  void AddObserver(Observer* observer);

  // Removes an observer.
  void RemoveObserver(Observer* observer);

 private:
  Id next_id_ = 0;
  base::flat_map<Id, std::unique_ptr<GuestOsMountProvider>> providers_;
  base::ObserverList<Observer> observers_;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_MOUNT_PROVIDER_REGISTRY_H_
