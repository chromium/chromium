// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_TAB_FEATURES_TEST_HELPER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_TAB_FEATURES_TEST_HELPER_H_

namespace tab_ranker {

struct TabFeatures;

// The following two functions are used in multiple tests to make sure the
// conversion, logging and inferencing use the same group of features.
// Returns a default tab features with some field unset.
TabFeatures GetPartialTabFeaturesForTesting();

// Returns a tab features with all field set.
TabFeatures GetFullTabFeaturesForTesting();

}  // namespace tab_ranker

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_TAB_FEATURES_TEST_HELPER_H_
