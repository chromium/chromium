// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/device_local_account_external_policy_loader.h"

#include <utility>

#include "base/values.h"

namespace chromeos {

DeviceLocalAccountExternalPolicyLoader::
    DeviceLocalAccountExternalPolicyLoader() = default;
DeviceLocalAccountExternalPolicyLoader::
    ~DeviceLocalAccountExternalPolicyLoader() = default;

void DeviceLocalAccountExternalPolicyLoader::StartLoading() {
  DCHECK(has_owner());

  // Through OnExtensionListsUpdated(), |prefs_| might have already loaded but
  // not consumed because we didn't have an owner then. Pass |prefs_| in that
  // case.
  if (prefs_)
    LoadFinished(std::move(prefs_));
}

void DeviceLocalAccountExternalPolicyLoader::OnExtensionListsUpdated(
    const base::DictionaryValue* prefs) {
  prefs_ = prefs->CreateDeepCopy();
  // Only call LoadFinished() when there is an owner to consume |prefs_|.
  if (has_owner())
    LoadFinished(std::move(prefs_));
}

}  // namespace chromeos
