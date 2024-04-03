// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OS_CRYPT_APP_BOUND_ENCRYPTION_WIN_H_
#define CHROME_BROWSER_OS_CRYPT_APP_BOUND_ENCRYPTION_WIN_H_

#include <string>

#include "base/win/windows_types.h"
#include "chrome/elevation_service/elevation_service_idl.h"

class PrefService;

namespace os_crypt {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SupportLevel {
  kSupported = 0,
  kNotSystemLevel = 1,
  kNotLocalDisk = 2,
  kApiFailed = 3,
  kNotUsingDefaultUserDataDir = 4,
  kUserDataDirNotLocalDisk = 5,
  kDisabledByPolicy = 6,
  kMaxValue = kDisabledByPolicy,
};

// Returns whether or not app-bound encryption is supported on the current
// platform configuration. If this does not return kSupported then Encrypt and
// Decrypt operations will fail. This can be called on any thread.
SupportLevel GetAppBoundEncryptionSupportLevel(PrefService* local_state);

// Encrypts a string with a Protection level of `level`. See
// `src/chrome/elevation_service/elevation-service_idl.idl` for the definition
// of available protection levels.
//
// This returns an HRESULT as defined by src/chrome/elevation_service/elevator.h
// or S_OK for success. If the call fails then `last_error` will be set to the
// value returned from the most recent failing Windows API call or
// ERROR_GEN_FAILURE.
//
// This should be called on a COM-enabled thread.
HRESULT EncryptAppBoundString(ProtectionLevel level,
                              const std::string& plaintext,
                              std::string& ciphertext,
                              DWORD& last_error);

// Decrypts a string previously encrypted by a call to EncryptAppBoundString.
//
// This returns an HRESULT as defined by src/chrome/elevation_service/elevator.h
// or S_OK for success. If the call fails then `last_error` will be set to the
// value returned from the most recent failing Windows API call or
// ERROR_GEN_FAILURE, and `log_message` may contain an extended log message, if
// supplied.
//
// This should be called on a COM-enabled thread.
HRESULT DecryptAppBoundString(const std::string& ciphertext,
                              std::string& plaintext,
                              DWORD& last_error,
                              std::string* log_message = nullptr);

// Allow non-standard user data dir for testing.
void SetNonStandardUserDataDirSupportedForTesting(bool supported);

}  // namespace os_crypt

#endif  // CHROME_BROWSER_OS_CRYPT_APP_BOUND_ENCRYPTION_WIN_H_
