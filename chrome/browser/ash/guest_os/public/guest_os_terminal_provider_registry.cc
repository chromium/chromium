// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_terminal_provider_registry.h"

#include <climits>
#include <memory>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/public/guest_os_terminal_provider.h"
#include "content/public/browser/browser_thread.h"

namespace guest_os {

GuestOsTerminalProviderRegistry::GuestOsTerminalProviderRegistry(
    Profile* profile)
    : profile_(profile) {}

GuestOsTerminalProviderRegistry::~GuestOsTerminalProviderRegistry() = default;

std::vector<GuestOsTerminalProviderRegistry::Id>
GuestOsTerminalProviderRegistry::List() {
  std::vector<Id> ret = std::vector<Id>();
  ret.reserve(providers_.size());
  for (const auto& pair : providers_) {
    ret.push_back(pair.first);
  }
  return ret;
}

GuestOsTerminalProvider* GuestOsTerminalProviderRegistry::Get(Id id) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto pos = providers_.find(id);
  if (pos == providers_.end()) {
    return nullptr;
  }
  return pos->second.get();
}

GuestOsTerminalProvider* GuestOsTerminalProviderRegistry::Get(
    const std::string& id) const {
  int output;
  if (!base::StringToInt(id, &output)) {
    return nullptr;
  }
  return Get(output);
}

GuestOsTerminalProvider* GuestOsTerminalProviderRegistry::Get(
    const guest_os::GuestId& id) const {
  for (const auto& pair : providers_) {
    if (pair.second->GuestId() == id) {
      return pair.second.get();
    }
  }
  return nullptr;
}

GuestOsTerminalProviderRegistry::Id GuestOsTerminalProviderRegistry::Register(
    std::unique_ptr<GuestOsTerminalProvider> provider) {
  // We use the range 0->INT_MAX because these IDs can get serialised into
  // base::Value, and that's the range they support.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(next_id_ < INT_MAX);
  Id id = next_id_++;
  providers_[id] = std::move(provider);
  SyncPrefs(id);

  return id;
}

void GuestOsTerminalProviderRegistry::SyncPrefs(
    GuestOsTerminalProviderRegistry::Id id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto pos = providers_.find(id);
  CHECK(pos != providers_.end());
  auto* provider = pos->second.get();

  // Per discussion on http://crrev/c/3774559, terminal app would like to read
  // prefs directly (via a private Chrome API) instead of getting data from
  // anywhere else, so update prefs too. First we add in case it doesn't already
  // exist, this is a no-op if it already does, then we update it.
  // Note: We only unset for explicit unregistration, if e.g. policy changes
  // while Chrome isn't running the pref doesn't get updated. Per the above CL
  // this is an explicit ask so they're still listed in the terminal app.
  AddContainerToPrefs(profile_, provider->GuestId(), {});
  UpdateContainerPref(profile_, provider->GuestId(),
                      prefs::kTerminalSupportedKey, base::Value(true));
  UpdateContainerPref(profile_, provider->GuestId(), prefs::kTerminalLabel,
                      base::Value(provider->Label()));
  UpdateContainerPref(profile_, provider->GuestId(),
                      prefs::kTerminalPolicyDisabled,
                      base::Value(!provider->AllowedByPolicy()));
}

std::unique_ptr<GuestOsTerminalProvider>
GuestOsTerminalProviderRegistry::Unregister(Id id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto pos = providers_.find(id);
  // No one should be unregistering random providers, so it's an error to try
  // and unregister one which doesn't exist rather than a no-op.
  CHECK(pos != providers_.end());
  auto ret = std::move(pos->second);
  providers_.erase(pos);

  // Per discussion on http://crrev/c/3774559, terminal app would like to read
  // prefs directly (via a private Chrome API) instead of getting data from
  // anywhere else, so update prefs to mark this guest as not supporting
  // terminal.
  // Note: We only unset for explicit unregistration, if e.g. policy changes
  // while Chrome isn't running the pref doesn't get updated. Per the above CL
  // this is an explicit ask so they're still listed in the terminal app.
  UpdateContainerPref(profile_, ret->GuestId(), prefs::kTerminalSupportedKey,
                      base::Value(false));
  return ret;
}

}  // namespace guest_os
