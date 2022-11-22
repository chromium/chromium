// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_FEATURES_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_FEATURES_H_

#include "base/feature_list.h"

namespace ash {

// Enables showing handoff screen to Family Link user during OOBE.
BASE_DECLARE_FEATURE(kFamilyLinkOobeHandoff);

bool IsFamilyLinkOobeHandoffEnabled();

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_FEATURES_H_
