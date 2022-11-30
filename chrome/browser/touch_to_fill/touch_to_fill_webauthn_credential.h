// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_WEBAUTHN_CREDENTIAL_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_WEBAUTHN_CREDENTIAL_H_

#include <string>

#include "base/types/strong_alias.h"

// Represents a Web Authentication credential to be displayed on the
// Touch-To-Fill sheet for user selection.
class TouchToFillWebAuthnCredential {
 public:
  using Username = base::StrongAlias<struct UsernameTag, std::u16string>;
  using BackendId = base::StrongAlias<struct BackendIdTag, std::string>;

  TouchToFillWebAuthnCredential(const Username& username,
                                const BackendId& backend_id);
  ~TouchToFillWebAuthnCredential();

  TouchToFillWebAuthnCredential(const TouchToFillWebAuthnCredential&);
  TouchToFillWebAuthnCredential& operator=(
      const TouchToFillWebAuthnCredential&);

  TouchToFillWebAuthnCredential(TouchToFillWebAuthnCredential&&);
  TouchToFillWebAuthnCredential& operator=(TouchToFillWebAuthnCredential&&);

  const Username& username() const { return username_; }

  const BackendId& id() const { return backend_id_; }

 private:
  Username username_;
  BackendId backend_id_;
};

bool operator==(const TouchToFillWebAuthnCredential& lhs,
                const TouchToFillWebAuthnCredential& rhs);

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_WEBAUTHN_CREDENTIAL_H_
