// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_UTIL_AFFILIATION_H_
#define CHROME_BROWSER_ENTERPRISE_UTIL_AFFILIATION_H_

#include "device_management_backend.pb.h"

namespace chrome {
namespace enterprise_util {

// Returns true if the profile and browser are managed by the same customer
// (affiliated). This is determined by comparing affiliation IDs obtained in the
// policy fetching response. If either policies has no affiliation IDs, this
// function returns false.
bool IsProfileAffiliated(
    const enterprise_management::PolicyData& profile_policy,
    const enterprise_management::PolicyData& browser_policy);

}  // namespace enterprise_util
}  // namespace chrome

#endif  // CHROME_BROWSER_ENTERPRISE_UTIL_AFFILIATION_H_
