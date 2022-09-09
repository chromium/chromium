// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_POWERWASH_REQUIREMENTS_CHECKER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_POWERWASH_REQUIREMENTS_CHECKER_H_

class Profile;

namespace policy {

// PowerwashRequirementsChecker is responsible to perform check if powerwash
// is required for current session and is able to show notifications to user.
// It is expected that there will be no devices requiring such check after
// November 2022 due to auto-updated expiration.
class PowerwashRequirementsChecker {
 public:
  // Context where PowerwashRequirementsChecker is used.
  enum class Context {
    kArc,
    kCrostini,
  };

  enum class State {
    // State is unknown. Could happen if cryptohome have not responded.
    kUndefined,
    // Powerwash is required.
    kRequired,
    // Powerwash is not required.
    kNotRequired,
  };

  // Asynchronously stores current cryptohome powerwash state shared
  // between future instances. Non-blocking call may intentionally lead to
  // race condition when state is still undefined. This case is handled in
  // |IsPowerwashRequired| and |ShowNotification|.
  static void Initialize();
  // Same as |Initialize| but blocks until initialization is finished.
  static void InitializeSynchronouslyForTesting();
  // Resets cryptohome powerwash state to unknown.
  static void ResetForTesting();

  PowerwashRequirementsChecker(Context context, Profile* profile);
  ~PowerwashRequirementsChecker() = default;
  PowerwashRequirementsChecker(const PowerwashRequirementsChecker&) = delete;
  PowerwashRequirementsChecker& operator=(const PowerwashRequirementsChecker&) =
      delete;

  // Returns powerwash state. Powerwash is REQUIRED if:
  //   (a) DeviceRebootOnUserSignout is not NEVER and
  //   (b) User is not affiliated and
  //   (c) Cryptohome requires powerwash
  // If (a) or (b) is not true then state is NOT_REQUIRED. If (a) and (b)
  // are true but (c) is undefined then state is UNDEFINED (e.g cryptohome has
  // not responded).
  State GetState() const;

  // Shows notification to user to powerwash the device.
  // If cryptohome powerwash state is undefined, error notification is shown
  // instead.
  void ShowNotification();

 private:
  // Returns true if DeviceRebootOnUserSignout is set.
  bool IsPolicySet() const;
  // Returns true if current user is affiliated.
  bool IsUserAffiliated() const;
  // Shows error notification in case when cryptohome failed to respond with
  // powerwash status.
  void ShowCryptohomeErrorNotification();

  const Context context_;

  Profile* const profile_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_POWERWASH_REQUIREMENTS_CHECKER_H_
