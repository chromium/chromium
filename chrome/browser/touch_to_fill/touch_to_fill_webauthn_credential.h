// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_WEBAUTHN_CREDENTIAL_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_WEBAUTHN_CREDENTIAL_H_

#include <string>

// Represents a Web Authentication credential to be displayed on the
// Touch-To-Fill sheet for user selection.
class TouchToFillWebAuthnCredential {
 public:
  TouchToFillWebAuthnCredential(const std::u16string& username,
                                const std::string& backend_id);
  ~TouchToFillWebAuthnCredential();

  TouchToFillWebAuthnCredential(const TouchToFillWebAuthnCredential&);
  TouchToFillWebAuthnCredential& operator=(
      const TouchToFillWebAuthnCredential&);

  TouchToFillWebAuthnCredential(TouchToFillWebAuthnCredential&&);
  TouchToFillWebAuthnCredential& operator=(TouchToFillWebAuthnCredential&&);

  const std::u16string& username() const { return username_; }

  const std::string& id() const { return backend_id_; }

 private:
  std::u16string username_;
  std::string backend_id_;
};

bool operator==(const TouchToFillWebAuthnCredential& lhs,
                const TouchToFillWebAuthnCredential& rhs);

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_WEBAUTHN_CREDENTIAL_H_
