// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_test_util.h"

#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/common/pref_names.h"

namespace supervised_user_test_util {

void AddCustodians(Profile* profile) {
  DCHECK(profile->IsChild());
  PrefService* prefs = profile->GetPrefs();
  prefs->SetString(prefs::kSupervisedUserCustodianEmail,
                   "test_parent_0@google.com");
  prefs->SetString(prefs::kSupervisedUserCustodianObfuscatedGaiaId,
                   "239029320");

  prefs->SetString(prefs::kSupervisedUserSecondCustodianEmail,
                   "test_parent_1@google.com");
  prefs->SetString(prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId,
                   "85948533");
}

}  // namespace supervised_user_test_util
