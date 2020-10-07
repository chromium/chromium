// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_FEATURES_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_FEATURES_H_

#include "base/feature_list.h"

namespace chromeos {

// Filters family user metrics into one of four types of family users.
// TODO(crbug/1103077): If any of the buckets end up being too small, disable
// this feature for privacy reasons.
extern const base::Feature kFamilyUserMetricsProvider;

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_FEATURES_H_
