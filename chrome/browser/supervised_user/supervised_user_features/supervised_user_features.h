// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_FEATURES_SUPERVISED_USER_FEATURES_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_FEATURES_SUPERVISED_USER_FEATURES_H_

#include "base/feature_list.h"

namespace supervised_users {

extern const base::Feature kEduCoexistenceFlowV2;

extern const base::Feature kLocalWebApprovals;

bool IsEduCoexistenceFlowV2Enabled();

bool IsLocalWebApprovalsEnabled();
}  // namespace supervised_users

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_FEATURES_SUPERVISED_USER_FEATURES_H_
