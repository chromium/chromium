// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_FEATURE_TOGGLER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_FEATURE_TOGGLER_H_

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

// Class that enables or disables a feature switch based on a gTest test
// parameter, intended to be used as a base class of the test fixture. Must be
// the first base class of the test fixture to take effect during the
// construction of the test fixture itself.
class FeatureToggler : public testing::WithParamInterface<bool> {
 public:
  explicit FeatureToggler(const base::Feature& feature);

  ~FeatureToggler();

 private:
  base::test::ScopedFeatureList override_features_;

  DISALLOW_COPY_AND_ASSIGN(FeatureToggler);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_FEATURE_TOGGLER_H_
