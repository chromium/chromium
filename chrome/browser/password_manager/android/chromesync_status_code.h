// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_CHROMESYNC_STATUS_CODE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_CHROMESYNC_STATUS_CODE_H_

namespace password_manager {

// TODO(crbug.com/40824450): Remove once GMS definition will be exposed.
// Status codes redefinition for the GMS ChromeSync API.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.password_manager
enum class ChromeSyncStatusCode {
  // We need user to retrieve their passphrase in order to decrypt the data.
  kPassphraseRequired = 11000,

  // Happens when user enters the wrong custom passphrase.
  kWrongPassphrase = 11001,

  // Happens when trying to retrieve / modify the data that is not being
  // subscribed by any subscribers.
  kDataNotSubscribed = 11002,

  // Happens when the execution of a ChromeSync service operation fails due to
  // the request/response data exceeding the binder size limit. Also see
  // https://developer.android.com/reference/android/os/TransactionTooLargeException
  kTransactionTooLarge = 11003,

  // Happens when the package of the calling application package name is not
  // allowlisted via the flag "first_party_api_allow_list".
  kAccessDenied = 11004,

  // Happens when the account is not authenticated and the issue is resolvable
  // by the user.
  kAuthErrorResolvable = 11005,

  // Happens when the account is not authenticated, but the issue is not
  // resolvable by the user.
  kAuthErrorUnresolvable = 11006,

  // The 0P API has some operations that don't support local accounts. If they
  // are called for local accounts, this status code is provided.
  kLocalAccountNotSupported = 11007,

  kMaxValue = kLocalAccountNotSupported,
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_CHROMESYNC_STATUS_CODE_H_
