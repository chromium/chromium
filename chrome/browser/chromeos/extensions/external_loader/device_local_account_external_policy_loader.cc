// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/external_loader/device_local_account_external_policy_loader.h"

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
  if (prefs_) {
    LoadFinished(std::move(prefs_).value());
    prefs_.reset();
  }
}

void DeviceLocalAccountExternalPolicyLoader::OnExtensionListsUpdated(
    const base::Value::Dict& prefs) {
  if (has_owner()) {
    LoadFinished(prefs.Clone());
    prefs_.reset();
    return;
  }

  prefs_ = prefs.Clone();
}

}  // namespace chromeos
