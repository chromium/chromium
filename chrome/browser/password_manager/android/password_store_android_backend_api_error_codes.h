// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_API_ERROR_CODES_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_API_ERROR_CODES_H_

// This enum mirrors enum PasswordStoreAndroidBackendAPIError in
// tools/metrcis/histograms/enums.xml. Do not reassign values and modify both
// files at the same time.
enum class AndroidBackendAPIErrorCode {
  kNetworkError = 7,
  kInternalError = 8,
  kDeveloperError = 10,
  kApiNotConnected = 17,
  kConnectionSuspendedDuringCall = 20,
  kReconnectionTimedOut = 22,
  kPassphraseRequired = 11000,
  kAccessDenied = 11004,
  kAuthErrorResolvable = 11005,
  kAuthErrorUnresolvable = 11006,
  kBadRequest = 11008,
  kBackendGeneric = 11009,
  kBackendResourceExhausted = 11010,
  kInvalidData = 11011,
  kUnmappedErrorCode = 11012,
  kUnexpectedError = 11013,
  kKeyRetrievalRequired = 11014,
  kChromeSyncAPICallError = 43502,
  kErrorWhileDoingLeakServiceGRPC = 43504,
  kRequiredSyncingAccountMissing = 43507,
  kLeakCheckServiceAuthError = 43508,
  kLeakCheckServiceResourceExhausted = 43509,
  kMaxValue = kLeakCheckServiceResourceExhausted,
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_API_ERROR_CODES_H_
