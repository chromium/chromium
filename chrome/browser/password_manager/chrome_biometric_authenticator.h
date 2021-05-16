// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_CHROME_BIOMETRIC_AUTHENTICATOR_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_CHROME_BIOMETRIC_AUTHENTICATOR_H_

#include "components/password_manager/core/browser/biometric_authenticator.h"

namespace content {
class WebContents;
}

// Chrome wrapper around BiometricAuthenticator. Subclasses are expected to
// provide an implementation for Create(), instantiating authenticators for a
// given platform.
class ChromeBiometricAuthenticator
    : public password_manager::BiometricAuthenticator {
 public:
  // Create an instance of the ChromeBiometricAuthenticator. Trying to use this
  // API on platforms that do not provide an implementation will result in a
  // link error. So far only Android provides an implementation.
  static scoped_refptr<password_manager::BiometricAuthenticator> Create(
      content::WebContents* web_contents);

 protected:
  ~ChromeBiometricAuthenticator() override = default;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_CHROME_BIOMETRIC_AUTHENTICATOR_H_
