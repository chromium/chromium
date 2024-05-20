// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_BLOCKED_APP_TYPES_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_BLOCKED_APP_TYPES_H_

#include <map>
#include <optional>
#include <string>

#include "base/time/time.h"

namespace ash::on_device_controls {

// State of the app in the context of on device app controls.
enum class LocalAppState {
  // App is not blocked by on device controls.
  kAvailable = 0,
  // App installed and blocked by on device controls.
  kBlocked = 1,
  // App uninstalled and blocked by on device controls.
  // Used to block the app upon reinstallation.
  kBlockedUninstalled = 2,
};

// The details of the app blocked on device.
// Note: Those details are preserved in-between the sessions.
class BlockedAppDetails {
 public:
  BlockedAppDetails();
  explicit BlockedAppDetails(base::Time block_timestamp);
  BlockedAppDetails(base::Time block_timestamp, base::Time uninstall_timestamp);

  ~BlockedAppDetails();

  base::Time block_timestamp() const { return block_timestamp_; }
  std::optional<base::Time> uninstall_timestamp() const {
    return uninstall_timestamp_;
  }

  // Returns whether the blocked app is currently installed.
  bool IsInstalled() const;
  // Marks app as installed by resetting the uninstall timestamp.
  void MarkInstalled();

  // Sets un-installation timestamp.
  // Marks app as uninstalled if that timestamp was not previously set.
  void SetUninstallTimestamp(base::Time timestamp);
  // Sets block timestamp.
  void SetBlockTimestamp(base::Time timestamp);

 private:
  // The timestamp when the app was blocked.
  base::Time block_timestamp_;

  // The timestamp when the app was uninstalled..
  // If not populated app is currently installed.
  std::optional<base::Time> uninstall_timestamp_;
};

// The map containing the record of the apps blocked on device.
// Keyed by the App Service app id.
typedef std::map<std::string, BlockedAppDetails> BlockedAppMap;

}  // namespace ash::on_device_controls

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_BLOCKED_APP_TYPES_H_
