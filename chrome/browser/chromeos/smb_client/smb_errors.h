// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_ERRORS_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_ERRORS_H_

#include "base/files/file.h"
#include "chromeos/dbus/smb_provider_client.h"

namespace chromeos {
namespace smb_client {

// These values are written to logs. New enum values may be added, but existing
// enums must never be renumbered or values reused. Must be kept in sync
// with the SmbMountResult enum in
// ui/webui/resources/cr_components/chromeos/smb_shares/smb_browser_proxy.js
// and the NativeSmbFileShare_MountResult enum in enums.xml.
enum class SmbMountResult {
  // TODO(allenvic): Change syntax to kConstantSyntax.
  SUCCESS = 0,                 // Mount succeeded.
  UNKNOWN_FAILURE = 1,         // Mount failed in an unrecognized way.
  AUTHENTICATION_FAILED = 2,   // Authentication to the share failed.
  NOT_FOUND = 3,               // The specified share was not found.
  UNSUPPORTED_DEVICE = 4,      // The specified share is not supported.
  MOUNT_EXISTS = 5,            // The specified share is already mounted.
  INVALID_URL = 6,             // The mount URL is an invalid SMB URL.
  INVALID_OPERATION = 7,       // libsmbclient returned invalid operation.
  DBUS_PARSE_FAILED = 8,       // Parsing the D-Bus message or response failed.
  OUT_OF_MEMORY = 9,           // The share is out of memory or storage.
  ABORTED = 10,                // The operation was aborted.
  IO_ERROR = 11,               // An I/O error occured.
  TOO_MANY_OPENED = 12,        // There are too many shares open.
  INVALID_SSO_URL = 13,        // The share URL is not valid when using SSO.
  kMaxValue = INVALID_SSO_URL  // Max enum value for use in metrics.
};

// Translates an smbprovider::ErrorType to a base::File::Error. Since
// smbprovider::ErrorType is a superset of base::File::Error, errors that do not
// map directly are logged and mapped to the generic failed error.
base::File::Error TranslateToFileError(smbprovider::ErrorType error);

// Translates a base::File::Error to an smbprovider::ErrorType. There is an
// explicit smbprovider::ErrorType for each base::File::Error.
smbprovider::ErrorType TranslateToErrorType(base::File::Error error);

// Translates an smbprovider::ErrorType to an SmbMountResult.
SmbMountResult TranslateErrorToMountResult(smbprovider::ErrorType error);

// Translates a base::File::Error to an SmbMountResult.
SmbMountResult TranslateErrorToMountResult(base::File::Error error);

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_SMB_ERRORS_H_
