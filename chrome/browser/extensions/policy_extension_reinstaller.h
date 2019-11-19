// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_POLICY_EXTENSION_REINSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_POLICY_EXTENSION_REINSTALLER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "extensions/common/extension_id.h"
#include "net/base/backoff_entry.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Class that asks ExtensionService to reinstall corrupted policy extensions.
// If a reinstallation fails for some reason (e.g. network unavailability) then
// it will retry reinstallation with backoff.
class PolicyExtensionReinstaller {
 public:
  using ReinstallCallback =
      base::RepeatingCallback<void(base::OnceClosure callback,
                                   base::TimeDelta delay)>;

  explicit PolicyExtensionReinstaller(content::BrowserContext* context);
  ~PolicyExtensionReinstaller();

  // Notifies this reinstaller about a policy extension corruption.
  void NotifyExtensionDisabledDueToCorruption();

  // For tests, overrides the default action to take to initiate policy
  // force-reinstalls.
  static void set_policy_reinstall_action_for_test(ReinstallCallback* action);

 private:
  void Fire();
  base::TimeDelta GetNextFireDelay();
  void ScheduleNextReinstallAttempt();

  content::BrowserContext* const context_ = nullptr;
  net::BackoffEntry backoff_entry_;
  // Whether or not there is a pending PostTask to Fire().
  bool scheduled_fire_pending_ = false;

  base::WeakPtrFactory<PolicyExtensionReinstaller> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PolicyExtensionReinstaller);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_POLICY_EXTENSION_REINSTALLER_H_
