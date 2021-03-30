// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_features.h"

#include "base/feature_list.h"

namespace supervised_users {

const base::Feature kSupervisedUserIframeFilter{
    "SupervisedUserIframeFilter", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEduCoexistenceFlowV2{"EduCoexistenceV2",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

bool IsEduCoexistenceFlowV2Enabled() {
  return base::FeatureList::IsEnabled(kEduCoexistenceFlowV2);
}

}  // namespace supervised_users
