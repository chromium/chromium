// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_TERMINAL_PROVIDER_REGISTRY_H_
#define CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_TERMINAL_PROVIDER_REGISTRY_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/guest_os/guest_id.h"

class Profile;

namespace guest_os {

class GuestOsTerminalProvider;

class GuestOsTerminalProviderRegistry {
 public:
  // Id used to reference a specific provider within the registry. Note: These
  // IDs are not stable across restarts and there's no correlation between a
  // provider's ID and its VM/Guest, or between different providers.
  // Use int and start at 0 so we can serialise it to json without any
  // conversions.
  using Id = int;

  // Gets a list of `Id`s for all provider's in the registry.
  std::vector<Id> List();

  explicit GuestOsTerminalProviderRegistry(Profile* profile);
  ~GuestOsTerminalProviderRegistry();
  GuestOsTerminalProviderRegistry(const GuestOsTerminalProviderRegistry&) =
      delete;
  GuestOsTerminalProviderRegistry& operator=(
      const GuestOsTerminalProviderRegistry&) = delete;

  // Returns the provider with the specified `id`. Returns nullptr if the
  // provider doesn't exist.
  GuestOsTerminalProvider* Get(Id id) const;

  // Returns the provider with the specified `id`. Returns nullptr if the
  // provider doesn't exist. Convenience method which converts std::string to
  // Id for you.
  GuestOsTerminalProvider* Get(const std::string& id) const;

  // Returns the provider with the specified `id`. Returns nullptr if the
  // provider doesn't exist. Searches the registry for the first provider for
  // the specified guest.
  GuestOsTerminalProvider* Get(const guest_os::GuestId& id) const;

  // Registers a new provider with the registry. The registry takes ownership of
  // the provider, holding on to it until it's unregistered. Returns the id of
  // the newly-registered provider.
  Id Register(std::unique_ptr<GuestOsTerminalProvider> provider);

  // The terminal reads configuration data from prefs, which means changes to
  // provider properties at runtime aren't automatically reflected in the
  // terminal window. This method updates prefs to match the current provider
  // state.
  void SyncPrefs(Id provider);

  // Removes a provider from the registry, returning the provider. The specified
  // provider must be in the registry.
  std::unique_ptr<GuestOsTerminalProvider> Unregister(Id provider);

 private:
  raw_ptr<Profile> profile_;
  Id next_id_ = 0;
  base::flat_map<Id, std::unique_ptr<GuestOsTerminalProvider>> providers_;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_TERMINAL_PROVIDER_REGISTRY_H_
