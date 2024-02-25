// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SERVICE_HELPER_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SERVICE_HELPER_H_

#include <string>
#include <vector>

#include "base/logging.h"

namespace ash::smb_client {

// Parse a user principal name into the user name and domain.
// The format is "user@domain.com", following RFC-822.
bool ParseUserPrincipalName(const std::string& user_principal_name,
                            std::string* user_name,
                            std::string* workgroup);

// Parse a down-level logon name into the user name and domain.
// The format is "DOMAIN\user". Note, this format has additional restrictions
// such as the domain is a NetBIOS domain (upper case, no .). However, these
// restrictions are not enforced.
bool ParseDownLevelLogonName(const std::string& logon_name,
                             std::string* user_name,
                             std::string* workgroup);

// Parse a user name, which can be in one of three formats:
// 1. Plain user name. i.e. "username"
// 2. User principal name. i.e. "username@domain.com"
// 3. Down-level logon name. i.e. "DOMAIN\username"
// The format is automatically detected. User principal and down-level logon
// names are documented at:
// https://docs.microsoft.com/en-au/windows/desktop/SecAuthN/user-name-formats
bool ParseUserName(const std::string& name,
                   std::string* user_name,
                   std::string* workgroup);

}  // namespace ash::smb_client

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_SMB_SERVICE_HELPER_H_
