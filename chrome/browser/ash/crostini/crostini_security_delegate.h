// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SECURITY_DELEGATE_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SECURITY_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/guest_os/guest_os_security_delegate.h"

class Profile;

namespace crostini {

class CrostiniSecurityDelegate : public guest_os::GuestOsSecurityDelegate {
 public:
  // Builds an instance of the security_delegate for the given |profile|.
  static void Build(
      Profile* profile,
      base::OnceCallback<
          void(std::unique_ptr<guest_os::GuestOsSecurityDelegate>)> callback);

  ~CrostiniSecurityDelegate() override;

  // exo::SecurityDelegate overrides:
  std::string GetSecurityContext() const override;
  bool CanLockPointer(aura::Window* window) const override;

 private:
  // Private constructor to force use of Build().
  CrostiniSecurityDelegate() = default;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_SECURITY_DELEGATE_H_
