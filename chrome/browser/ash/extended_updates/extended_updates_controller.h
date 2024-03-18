// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_CONTROLLER_H_
#define CHROME_BROWSER_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_CONTROLLER_H_

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

// Params struct used as input to extended updates eligibility check function.
// |eol_passed| is true if the device passed its auto update expiration date.
// |extended_date_passed| is true if the device passed its extended update date.
// |opt_in_required| is true if the device requires user opt-in to receive
// extended updates.
struct ExtendedUpdatesParams {
  bool eol_passed = false;
  bool extended_date_passed = false;
  bool opt_in_required = false;
};

// Whether the device is eligible to opt-in for extended updates.
// This depends on multiple criteria, e.g. whether opt-in is required,
// being within the allowed time window, the user type, whether the device
// is already opted in.
// |context| is the Profile of the current user.
bool IsExtendedUpdatesOptInEligible(content::BrowserContext* context,
                                    const ExtendedUpdatesParams& params);

// Whether the device is eligible to opt-in for extended updates.
// This version assumes the values in ExtendedUpdatesParams are eligible.
// TODO(b/330230644): Consolidate with above function.
bool IsExtendedUpdatesOptInEligible(content::BrowserContext* context);

// Whether the device is opted in for receiving extended updates.
bool IsExtendedUpdatesOptedIn();

// Opts the device into receiving extended updates.
// Returns true if the operation succeeded.
// The caller should check for eligibility before calling this.
bool OptInExtendedUpdates(content::BrowserContext* context);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_CONTROLLER_H_
