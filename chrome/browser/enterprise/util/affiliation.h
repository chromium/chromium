// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_UTIL_AFFILIATION_H_
#define CHROME_BROWSER_ENTERPRISE_UTIL_AFFILIATION_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "device_management_backend.pb.h"

class Profile;

namespace chrome {
namespace enterprise_util {

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OS_ANDROID)

// Returns the PolicyData corresponding to |profile|, or nullptr if it can't be
// obtained.
const enterprise_management::PolicyData* GetProfilePolicyData(Profile* profile);

// Returns the PolicyData corresponding to the browser, or nullptr if it can't
// be obtained.
const enterprise_management::PolicyData* GetBrowserPolicyData();

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OS_ANDROID)

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
