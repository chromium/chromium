// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_SCOPED_GEIC_TEST_FEATURES_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_SCOPED_GEIC_TEST_FEATURES_H_

#include "base/test/scoped_feature_list.h"

namespace glic {

// A valid Gemini Enterprise side-panel guest URL for tests.
inline constexpr char kTestGeicGuestUrl[] =
    "https://business.gemini.google/side-panel?configId=test";

// Enables GEiC (`features::kGeic`) with `kTestGeicGuestUrl` as the guest URL
// for the lifetime of this object, and also `features::kGeicFre` if
// `enable_fre` is true. Doesn't enable `features::kGlic`.
//
// Create it before the profile, e.g. as a browser test fixture member, so
// that the profile is latched to Gemini Enterprise.
class ScopedGeicTestFeatures {
 public:
  explicit ScopedGeicTestFeatures(bool enable_fre);
  ScopedGeicTestFeatures(const ScopedGeicTestFeatures&) = delete;
  ScopedGeicTestFeatures& operator=(const ScopedGeicTestFeatures&) = delete;
  ~ScopedGeicTestFeatures();

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_SCOPED_GEIC_TEST_FEATURES_H_
