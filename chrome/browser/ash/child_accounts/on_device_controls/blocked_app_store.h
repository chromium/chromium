// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_BLOCKED_APP_STORE_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_BLOCKED_APP_STORE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/blocked_app_types.h"

class PrefRegistrySimple;
class PrefService;

namespace ash::on_device_controls {

// Persists blocked apps in the user pref and provides API to load and update
// persisted data.
class BlockedAppStore {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit BlockedAppStore(PrefService* pref_service);
  BlockedAppStore(const BlockedAppStore&) = delete;
  BlockedAppStore& operator=(const BlockedAppStore&) = delete;
  ~BlockedAppStore();

  // Returns the list of blocked apps stored in pref.
  BlockedAppMap GetFromPref() const;
  // Saves the list of blocked `apps` in pref.
  void SaveToPref(const BlockedAppMap& apps);

 private:
  const raw_ptr<PrefService> pref_service_;
};

}  // namespace ash::on_device_controls

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_BLOCKED_APP_STORE_H_
