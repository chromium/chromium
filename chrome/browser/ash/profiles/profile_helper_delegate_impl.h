// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PROFILES_PROFILE_HELPER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_PROFILES_PROFILE_HELPER_DELEGATE_IMPL_H_

#include "chrome/browser/ash/profiles/profile_helper.h"

namespace ash {

// Injects chrome/browser dependency to ProfileHelper.
// TODO(crbug.com/1325210): Remove g_browser_process dependency from this
// implementation, which requires to change the lifetime of the instance.
class ProfileHelperDelegateImpl : public ProfileHelper::Delegate {
 public:
  ProfileHelperDelegateImpl();
  ~ProfileHelperDelegateImpl() override;

  // ProfileHelper::Delegate overrides
  Profile* GetProfileByPath(const base::FilePath& path) override;
  Profile* DeprecatedGetProfile(const base::FilePath& path) override;
  const base::FilePath* GetUserDataDir() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PROFILES_PROFILE_HELPER_DELEGATE_IMPL_H_
