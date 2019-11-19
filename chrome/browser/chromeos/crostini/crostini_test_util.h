// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_TEST_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_TEST_UTIL_H_

namespace crostini {

// Sets up environment to allow dialog creation in tests.
void SetUpViewsEnvironmentForTesting();

// Tears down environment that allowed dialog creation in tests.
void TearDownViewsEnvironmentForTesting();

void CloseCrostiniAnsibleSoftwareConfigViewForTesting();

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_TEST_UTIL_H_
