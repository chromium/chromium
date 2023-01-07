// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_RESET_METRICS_H_
#define CHROME_BROWSER_ASH_RESET_METRICS_H_

namespace ash {
namespace reset {

enum class DialogViewType {

  // User invoked the dialog from options page.
  kFromOptions,

  // Invoked with shortcut. Confirming form for powerwash.
  kShortcutConfirmingPowerwashOnly,

  // Invoked with shortcut. Confirming form for powerwash and rollback.
  kShortcutConfirmingPowerwashAndRollback,

  // Invoked with shortcut. Offering form, rollback option set.
  kShortcutOfferingRollbackUnavailable,

  // Invoked with shortcut. Offering form, rollback option not set.
  kShortcutOfferingRollbackAvailable,

  // Invoked with shortcut. Requesting restart form.
  kShortcutRestartRequired,

  // Must be last enum element.
  kCount
};

}  // namespace reset
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_RESET_METRICS_H_
