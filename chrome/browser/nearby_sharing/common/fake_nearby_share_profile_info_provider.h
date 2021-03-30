// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_COMMON_FAKE_NEARBY_SHARE_PROFILE_INFO_PROVIDER_H_
#define CHROME_BROWSER_NEARBY_SHARING_COMMON_FAKE_NEARBY_SHARE_PROFILE_INFO_PROVIDER_H_

#include "chrome/browser/nearby_sharing/common/nearby_share_profile_info_provider.h"

class FakeNearbyShareProfileInfoProvider
    : public NearbyShareProfileInfoProvider {
 public:
  FakeNearbyShareProfileInfoProvider();
  ~FakeNearbyShareProfileInfoProvider() override;

  // NearbyShareProfileInfoProvider:
  base::Optional<std::u16string> GetGivenName() const override;
  base::Optional<std::string> GetProfileUserName() const override;

  void set_given_name(const base::Optional<std::u16string>& given_name) {
    given_name_ = given_name;
  }
  void set_profile_user_name(
      const base::Optional<std::string>& profile_user_name) {
    profile_user_name_ = profile_user_name;
  }

 private:
  base::Optional<std::u16string> given_name_;
  base::Optional<std::string> profile_user_name_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_COMMON_FAKE_NEARBY_SHARE_PROFILE_INFO_PROVIDER_H_
