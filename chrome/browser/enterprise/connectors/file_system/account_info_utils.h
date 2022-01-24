// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_ACCOUNT_INFO_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_ACCOUNT_INFO_UTILS_H_

#include "chrome/browser/enterprise/connectors/common.h"
#include "components/prefs/pref_service.h"

class PrefRegistrySimple;

namespace enterprise_connectors {

// The unique key for the Box service provider.  This is used to generate the
// correct network annotation tag as well as possible parameters in the
// access token consent URL.
constexpr char kFileSystemServiceProviderPrefNameBox[] = "box";

// Registers all the preferences needed to support the given service provider
// for use with the file system connector.
void RegisterFileSystemPrefsForServiceProvider(
    PrefRegistrySimple* registry,
    const std::string& service_provider);

// Stores the OAuth2 tokens for the given service provider.  Returns true
// if both tokens were successfully stored and false if either store fails.
bool SetFileSystemOAuth2Tokens(PrefService* prefs,
                               const std::string& service_provider,
                               const std::string& access_token,
                               const std::string& refresh_token);

// Clears the OAuth2 tokens for the given service provider.
bool ClearFileSystemAccessToken(PrefService* prefs,
                                const std::string& service_provider);
bool ClearFileSystemOAuth2Tokens(PrefService* prefs,
                                 const std::string& service_provider);

// Retrieves the OAuth2 tokens for the given service provider.  If a token
// argument is null that token is not retrieved.  Returns true if all requested
// tokens are retrieved and false if any fail.
bool GetFileSystemOAuth2Tokens(PrefService* prefs,
                               const std::string& service_provider,
                               std::string* access_token,
                               std::string* refresh_token);
// Retrieves a list of prefs paths for connector account info.
std::vector<std::string> GetFileSystemConnectorAccountInfoPrefs(
    const std::string& service_provider);

// Stores/retrieves/clears the default folder id and name stored for the given
// service provider.
void SetDefaultFolder(PrefService* prefs,
                      const std::string& service_provider,
                      std::string folder_id,
                      std::string folder_name);
void ClearDefaultFolder(PrefService* prefs,
                        const std::string& service_provider);
std::string GetDefaultFolderId(PrefService* prefs,
                               const std::string& service_provider);
std::string GetDefaultFolderLink(PrefService* prefs,
                                 const std::string& service_provider);
std::string GetDefaultFolderName(PrefService* prefs,
                                 const std::string& service_provider);

// Stores/retrieves/clears the account info stored for the given service
// provider.
void SetFileSystemAccountInfo(PrefService* prefs,
                              const std::string& service_provider,
                              base::Value account_info);
bool ClearFileSystemAccountInfo(PrefService* prefs,
                                const std::string& service_provider);
base::Value GetFileSystemAccountInfo(PrefService* prefs,
                                     const std::string& service_provider);

struct AccountInfo {
  std::string account_name;
  std::string account_login;
  std::string folder_link;
  std::string folder_name;

  AccountInfo();
  ~AccountInfo();
  AccountInfo(const AccountInfo& other);
};

absl::optional<AccountInfo> GetFileSystemAccountInfoFromPrefs(
    const FileSystemSettings& settings,
    PrefService* prefs);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_ACCOUNT_INFO_UTILS_H_
