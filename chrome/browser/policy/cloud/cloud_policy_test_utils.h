// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_TEST_UTILS_H_
#define CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_TEST_UTILS_H_

namespace policy {

class PolicyMap;

// Fills in the PolicyMap with all of the default policies for an enterprise
// user. If any of the default policies already have entries in the PolicyMap,
// this routine will not overwrite them (it only adds entries that do not
// currently exist in the map).
void GetExpectedDefaultPolicy(PolicyMap* policy_map);

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_CLOUD_POLICY_TEST_UTILS_H_
