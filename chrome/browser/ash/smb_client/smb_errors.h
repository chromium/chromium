// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_SMB_ERRORS_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_SMB_ERRORS_H_

#include "base/files/file.h"
#include "chromeos/ash/components/dbus/smbprovider/smb_provider_client.h"

namespace ash::smb_client {

// These values are written to logs. New enum values may be added, but existing
// enums must never be renumbered or values reused. Must be kept in sync
// with the SmbMountResult enum in
// ash/webui/common/resources/smb_shares/smb_browser_proxy.js
// and the NativeSmbFileShare_MountResult enum in enums.xml.
enum class SmbMountResult {
  kSuccess = 0,                 // Mount succeeded.
  kUnknownFailure = 1,          // Mount failed in an unrecognized way.
  kAuthenticationFailed = 2,    // Authentication to the share failed.
  kNotFound = 3,                // The specified share was not found.
  kUnsupportedDevice = 4,       // The specified share is not supported.
  kMountExists = 5,             // The specified share is already mounted.
  kInvalidUrl = 6,              // The mount URL is an invalid SMB URL.
  kInvalidOperation = 7,        // libsmbclient returned invalid operation.
  kDbusParseFailed = 8,         // Parsing the D-Bus message or response failed.
  kOutOfMemory = 9,             // The share is out of memory or storage.
  kAborted = 10,                // The operation was aborted.
  kIoError = 11,                // An I/O error occurred.
  kTooManyOpened = 12,          // There are too many shares open.
  kInvalidSsoUrl = 13,          // The share URL is not valid when using SSO.
  kInvalidUsername = 14,        // The username is invalid.
  kMaxValue = kInvalidUsername  // Max enum value for use in metrics.
};

// Translates an smbprovider::ErrorType to an SmbMountResult.
SmbMountResult TranslateErrorToMountResult(smbprovider::ErrorType error);

}  // namespace ash::smb_client

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_SMB_ERRORS_H_
