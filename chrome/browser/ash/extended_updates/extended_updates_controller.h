// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_CONTROLLER_H_
#define CHROME_BROWSER_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_CONTROLLER_H_

#include <compare>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ownership {
class OwnerSettingsService;
}  // namespace ownership

namespace ash {

// Controller for interacting with Extended Updates functionality.
class ExtendedUpdatesController {
 public:
  // Params struct used as input to extended updates eligibility check function.
  // |eol_passed| whether the device passed its auto update expiration date.
  // |extended_date_passed| whether the device passed its extended updates date.
  // |opt_in_required| whether the device requires user opt-in to receive
  // extended updates.
  struct Params {
    bool eol_passed = false;
    bool extended_date_passed = false;
    bool opt_in_required = false;

    auto operator<=>(const Params&) const = default;
  };

  ExtendedUpdatesController(const ExtendedUpdatesController&) = delete;
  ExtendedUpdatesController& operator=(const ExtendedUpdatesController&) =
      delete;
  virtual ~ExtendedUpdatesController();

  // Getter for the global controller instance.
  // A new instance is created if one doesn't exist already.
  static ExtendedUpdatesController* Get();

  // Whether the device is eligible to opt-in for extended updates.
  // This depends on multiple criteria, e.g. whether opt-in is required,
  // being within the allowed time window, the user type, whether the device
  // is already opted in.
  // |context| is the Profile of the current user.
  // |params| contains the other input parameters.
  virtual bool IsOptInEligible(content::BrowserContext* context,
                               const Params& params);

  // Whether the device is eligible to opt-in for extended updates.
  // This version assumes the values in Params are eligible.
  // TODO(b/330230644): Consolidate with above function.
  bool IsOptInEligible(content::BrowserContext* context);

  // Whether the device is opted in for receiving extended updates.
  virtual bool IsOptedIn();

  // Opts the device into receiving extended updates.
  // Returns true if the operation succeeded.
  // The caller should check for eligibility before calling this.
  bool OptIn(content::BrowserContext* context);

  // Called when EolInfo is fetched.
  virtual void OnEolInfo(content::BrowserContext* context,
                         const UpdateEngineClient::EolInfo& eol_info);

  void SetClockForTesting(base::Clock* clock);

 protected:
  ExtendedUpdatesController();

  void MaybeShowNotification(base::WeakPtr<content::BrowserContext> context);

 private:
  friend class ScopedExtendedUpdatesController;

  // Helper function to set the global controller instance for testing.
  // Returns the previous controller instance.
  // Tests should not call this directly; use ScopedExtendedUpdatesController
  // instead.
  static ExtendedUpdatesController* SetInstanceForTesting(
      ExtendedUpdatesController* controller);

  // Returns true if the user has the ability to opt in the device.
  bool HasOptInAbility(ownership::OwnerSettingsService* owner_settings);

  bool ShouldShowNotification(content::BrowserContext* context);

  void ShowNotification(content::BrowserContext* context);

  raw_ptr<base::Clock> clock_ = nullptr;

  base::WeakPtrFactory<ExtendedUpdatesController> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EXTENDED_UPDATES_EXTENDED_UPDATES_CONTROLLER_H_
