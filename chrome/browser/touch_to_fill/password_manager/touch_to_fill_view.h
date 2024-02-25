// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_TOUCH_TO_FILL_VIEW_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_TOUCH_TO_FILL_VIEW_H_

#include "base/containers/span.h"
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

  enum ShowFlags {
    kNone = 0,

    // Indicates whether Touch To Fill will submit a form after filling.
    kTriggerSubmission = 1 << 0,

    // Indicates whether selecting the 'manage' button with passkeys available
    // will show a screen that also allows management of passwords.
    kCanManagePasswordsWhenPasskeysPresent = 1 << 1,

    // Indicates whether the footer should contain a button that invokes hybrid
    // passkey sign-in.
    kShouldShowHybridOption = 1 << 2,

    // Indicates if there should be a list item to open Android Credential
    // Manager UI.
    kShouldShowCredManEntry = 1 << 3,
  };

  TouchToFillView() = default;
  TouchToFillView(const TouchToFillView&) = delete;
  TouchToFillView& operator=(const TouchToFillView&) = delete;
  virtual ~TouchToFillView() = default;

  // Instructs Touch To Fill to show the provided `credentials` to the user.
  // `formatted_url` contains a human friendly version of the current origin.
  // `is_origin_secure` indicates whether the current frame origin is secure.
  // `flags` is a combination of bits that affect the behaviors listed in the
  // `ShowFlags` enum. After user interaction either OnCredentialSelected() or
  // OnDismiss() gets invoked.
  virtual bool Show(
      const GURL& url,
      IsOriginSecure is_origin_secure,
      base::span<const password_manager::UiCredential> credentials,
      base::span<const password_manager::PasskeyCredential> passkey_credentials,
      int flags) = 0;

  // Invoked in case the user chooses an entry from the credential list
  // presented to them.
  virtual void OnCredentialSelected(
      const password_manager::UiCredential& credential) = 0;

  // Invoked if the user dismissed the Touch To Fill sheet without choosing a
  // credential.
  virtual void OnDismiss() = 0;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_TOUCH_TO_FILL_VIEW_H_
