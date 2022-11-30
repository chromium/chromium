// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OS_CRYPT_APP_BOUND_ENCRYPTION_WIN_H_
#define CHROME_BROWSER_OS_CRYPT_APP_BOUND_ENCRYPTION_WIN_H_

#include <string>

#include "base/win/windows_types.h"
#include "chrome/elevation_service/elevation_service_idl.h"

namespace os_crypt {

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
// ERROR_GEN_FAILURE.
//
// This should be called on a COM-enabled thread.
HRESULT DecryptAppBoundString(const std::string& ciphertext,
                              std::string& plaintext,
                              DWORD& last_error);

}  // namespace os_crypt

#endif  // CHROME_BROWSER_OS_CRYPT_APP_BOUND_ENCRYPTION_WIN_H_
