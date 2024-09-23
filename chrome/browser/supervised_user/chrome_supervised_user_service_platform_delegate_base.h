// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHROME_SUPERVISED_USER_SERVICE_PLATFORM_DELEGATE_BASE_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHROME_SUPERVISED_USER_SERVICE_PLATFORM_DELEGATE_BASE_H_

#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"

class Profile;

// Base class for the Chrome implementations of
// supervised_user::SupervisedUserService::PlatformDelegate.
class ChromeSupervisedUserServicePlatformDelegateBase
    : public supervised_user::SupervisedUserService::PlatformDelegate,
      public ProfileObserver {
 public:
  explicit ChromeSupervisedUserServicePlatformDelegateBase(Profile& profile);
  ~ChromeSupervisedUserServicePlatformDelegateBase() override;

  // supervised_user::SupervisedUserService::PlatformDelegate::
  std::string GetCountryCode() const override;
  version_info::Channel GetChannel() const override;

  // ProfileObserver::
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;

 protected:
  raw_ref<Profile> profile_;

 private:
  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      profile_observations_{this};
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHROME_SUPERVISED_USER_SERVICE_PLATFORM_DELEGATE_BASE_H_
