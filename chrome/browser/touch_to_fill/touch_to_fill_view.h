// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_VIEW_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_VIEW_H_

#include "base/containers/span.h"
#include "base/strings/string_piece_forward.h"
#include "base/types/strong_alias.h"
#include "url/gurl.h"

namespace password_manager {
class PasskeyCredential;
class UiCredential;
}

// This class represents the interface used for communicating between the Touch
// To Fill controller with the Android frontend.
class TouchToFillView {
 public:
  using IsOriginSecure = base::StrongAlias<class IsOriginSecureTag, bool>;

  TouchToFillView() = default;
  TouchToFillView(const TouchToFillView&) = delete;
  TouchToFillView& operator=(const TouchToFillView&) = delete;
  virtual ~TouchToFillView() = default;

  // Instructs Touch To Fill to show the provided |credentials| to the user.
  // |formatted_url| contains a human friendly version of the current origin.
  // |is_origin_secure| indicates whether the current frame origin is secure.
  // |trigger_submission| indicates whether Touch To Fill will submit a form
  // after filling. After user interaction either OnCredentialSelected() or
  // OnDismiss() gets invoked.
  virtual void Show(
      const GURL& url,
      IsOriginSecure is_origin_secure,
      base::span<const password_manager::UiCredential> credentials,
      base::span<const password_manager::PasskeyCredential> passkey_credentials,
      bool trigger_submission) = 0;

  // Invoked in case the user chooses an entry from the credential list
  // presented to them.
  virtual void OnCredentialSelected(
      const password_manager::UiCredential& credential) = 0;

  // Invoked if the user dismissed the Touch To Fill sheet without choosing a
  // credential.
  virtual void OnDismiss() = 0;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_VIEW_H_
