// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXO_CHROME_SECURITY_DELEGATE_H_
#define CHROME_BROWSER_ASH_EXO_CHROME_SECURITY_DELEGATE_H_

#include "components/exo/security_delegate.h"

namespace ash {

class ChromeSecurityDelegate : public exo::SecurityDelegate {
 public:
  ChromeSecurityDelegate();
  ChromeSecurityDelegate(const ChromeSecurityDelegate&) = delete;
  ChromeSecurityDelegate& operator=(const ChromeSecurityDelegate&) = delete;
  ~ChromeSecurityDelegate() override;

  // exo::SecurityDelegate;
  bool CanSelfActivate(aura::Window* window) const override;
  bool CanLockPointer(aura::Window* window) const override;
  SetBoundsPolicy CanSetBounds(aura::Window* window) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EXO_CHROME_SECURITY_DELEGATE_H_
