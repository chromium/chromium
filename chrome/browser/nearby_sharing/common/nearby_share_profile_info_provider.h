// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_PROFILE_INFO_PROVIDER_H_
#define CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_PROFILE_INFO_PROVIDER_H_

#include <optional>
#include <string>

// A delegate class that returns the Profile information necessary for
// browser-independent Nearby Share components, such as the local-device-data
// and contacts managers.
class NearbyShareProfileInfoProvider {
 public:
  NearbyShareProfileInfoProvider() = default;
  virtual ~NearbyShareProfileInfoProvider() = default;

  // Proxy for User::GetGivenName(). Returns std::nullopt if a valid given name
  // cannot be returned.
  virtual std::optional<std::u16string> GetGivenName() const = 0;

  // Proxy for Profile::GetProfileUserName(). Returns std::nullopt if a valid
  // user name cannot be returned.
  virtual std::optional<std::string> GetProfileUserName() const = 0;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_PROFILE_INFO_PROVIDER_H_
