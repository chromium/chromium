// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_UTIL_AFFILIATION_H_
#define CHROME_BROWSER_ENTERPRISE_UTIL_AFFILIATION_H_

class Profile;

namespace chrome {
namespace enterprise_util {

// Returns true if the profile and browser are managed by the same customer
// (affiliated). This is determined by comparing affiliation IDs obtained in the
// policy fetching response. If either policies has no affiliation IDs, this
// function returns false.
bool IsProfileAffiliated(Profile* profile);

}  // namespace enterprise_util
}  // namespace chrome

#endif  // CHROME_BROWSER_ENTERPRISE_UTIL_AFFILIATION_H_
