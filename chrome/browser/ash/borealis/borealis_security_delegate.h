// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SECURITY_DELEGATE_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SECURITY_DELEGATE_H_

#include <memory>
#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/guest_os/guest_os_security_delegate.h"

class Profile;

namespace borealis {

// Controls what specialized behaviours the exo server will have when dealing
// with borealis clients.
class BorealisSecurityDelegate : public guest_os::GuestOsSecurityDelegate {
 public:
  // Builds an instance of the security_delegate for the given |profile|.
  static void Build(
      Profile* profile,
      base::OnceCallback<
          void(std::unique_ptr<guest_os::GuestOsSecurityDelegate>)> callback);

  ~BorealisSecurityDelegate() override;

  // exo::SecurityDelegate overrides:
  bool CanSelfActivate(aura::Window* window) const override;
  bool CanLockPointer(aura::Window* window) const override;
  bool CanSetBoundsWithServerSideDecoration(
      aura::Window* window) const override;

  // Used in tests to avoid the async Build() call.
  static std::unique_ptr<BorealisSecurityDelegate> MakeForTesting(
      Profile* profile);

 private:
  // Private constructor, use Build().
  explicit BorealisSecurityDelegate(Profile* profile);

  Profile* const profile_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SECURITY_DELEGATE_H_
