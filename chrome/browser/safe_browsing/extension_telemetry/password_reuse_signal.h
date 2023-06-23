// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_PASSWORD_REUSE_SIGNAL_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_PASSWORD_REUSE_SIGNAL_H_

#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_service.h"

namespace safe_browsing {

// A signal that is created when a password reuse event is detected within an
// extension page.
class PasswordReuseSignal : public ExtensionSignal {
 public:
  PasswordReuseSignal(const extensions::ExtensionId& extension_id,
                      PasswordReuseInfo password_reuse_info);
  ~PasswordReuseSignal() override;

  // ExtensionSignal:
  ExtensionSignalType GetType() const override;

  uint64_t reused_password_hash() {
    return password_reuse_info_.reused_password_hash;
  }

  const PasswordReuseInfo password_reuse_info() const {
    return password_reuse_info_;
  }

 protected:
  PasswordReuseInfo password_reuse_info_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_PASSWORD_REUSE_SIGNAL_H_
