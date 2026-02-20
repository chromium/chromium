// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_MANAGEMENT_IDENTITY_H_
#define CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_MANAGEMENT_IDENTITY_H_

#include <optional>
#include <string>

class Profile;

class ScopedDeviceManagerForTesting {
 public:
  explicit ScopedDeviceManagerForTesting(const char* manager);
  ~ScopedDeviceManagerForTesting();

 private:
  const char* previous_manager_ = nullptr;
};

// Returns the enterprise domain of `profile` if one was found.
// This function will try to get the hosted domain and fallback on the domain
// of the email of the signed in account.
std::optional<std::string> GetEnterpriseAccountDomain(const Profile& profile);

// Returns nullopt if the device is not managed, the UTF8-encoded string
// representation of the manager identity if available and an empty string if
// the device is managed but the manager is not known or if the policy store
// hasn't been loaded yet.
std::optional<std::string> GetDeviceManagerIdentity();

// Returns the UTF8-encoded string representation of the the entity that manages
// `profile` or nullopt if unmanaged. For standard dasher domains, this will be
// a domain name (ie foo.com). For FlexOrgs, this will be the email address of
// the admin of the FlexOrg (ie user@foo.com). If DMServer does not provide this
// information, this function defaults to the domain of the account.
// TODO(crbug.com/40130449): Refactor localization hints for all strings that
// depend on this function.
std::optional<std::string> GetAccountManagerIdentity(Profile* profile);

#endif  // CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_MANAGEMENT_IDENTITY_H_
