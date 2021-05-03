// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_REPORTING_MANAGER_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_REPORTING_MANAGER_TEST_HELPER_H_

#include "testing/gtest/include/gtest/gtest.h"

class DlpPolicyEvent;

::testing::Matcher<const DlpPolicyEvent&> IsDlpPolicyEvent(
    const DlpPolicyEvent& event);
DlpPolicyEvent CreatePrintingRestrictedDlpEvent(const std::string& src_pattern);

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_REPORTING_MANAGER_TEST_HELPER_H_
