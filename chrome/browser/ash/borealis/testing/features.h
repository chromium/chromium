// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_TESTING_FEATURES_H_
#define CHROME_BROWSER_ASH_BOREALIS_TESTING_FEATURES_H_

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/user_manager/scoped_user_manager.h"

namespace ash {
class FakeChromeUserManager;
}

class Profile;

namespace borealis {

void AllowBorealis(Profile* profile,
                   base::test::ScopedFeatureList* features,
                   ash::FakeChromeUserManager* user_manager,
                   bool also_enable);

class ScopedAllowBorealis {
 public:
  ScopedAllowBorealis(Profile* profile, bool also_enable);
  ~ScopedAllowBorealis();

 private:
  raw_ptr<Profile, ExperimentalAsh> profile_;
  base::test::ScopedFeatureList features_;
  user_manager::ScopedUserManager user_manager_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_TESTING_FEATURES_H_
