// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CORRUPTED_EXTENSION_REINSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_CORRUPTED_EXTENSION_REINSTALLER_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "extensions/common/extension_id.h"
#include "net/base/backoff_entry.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Class that asks ExtensionService to reinstall corrupted extensions.
// If a reinstallation fails for some reason (e.g. network unavailability) then
// it will retry reinstallation with backoff.
class CorruptedExtensionReinstaller {
 public:
  using ReinstallCallback =
      base::RepeatingCallback<void(base::OnceClosure callback,
                                   base::TimeDelta delay)>;

  explicit CorruptedExtensionReinstaller(content::BrowserContext* context);

  CorruptedExtensionReinstaller(const CorruptedExtensionReinstaller&) = delete;
  CorruptedExtensionReinstaller& operator=(
      const CorruptedExtensionReinstaller&) = delete;

  ~CorruptedExtensionReinstaller();

  // Notifies this reinstaller about an extension corruption.
  void NotifyExtensionDisabledDueToCorruption();

  // Called when ExtensionSystem is shutting down. Cancels already-scheduled
  // attempts, if any, for a smoother shutdown.
  void Shutdown();

  // For tests, overrides the default action to take to initiate reinstalls.
  static void set_reinstall_action_for_test(ReinstallCallback* action);

 private:
  void Fire();
  base::TimeDelta GetNextFireDelay();
  void ScheduleNextReinstallAttempt();

  const raw_ptr<content::BrowserContext> context_ = nullptr;
  net::BackoffEntry backoff_entry_;
  // Whether or not there is a pending PostTask to Fire().
  bool scheduled_fire_pending_ = false;

  base::WeakPtrFactory<CorruptedExtensionReinstaller> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CORRUPTED_EXTENSION_REINSTALLER_H_
