// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_SCOPED_PREWARM_FEATURE_LIST_H_
#define CHROME_BROWSER_PRELOADING_SCOPED_PREWARM_FEATURE_LIST_H_

#include "base/test/scoped_feature_list.h"
#include "url/gurl.h"

namespace test {

// Enables or disables appropriate features for DSE Prewarm.
class ScopedPrewarmFeatureList {
 public:
  enum class PrewarmState {
    kDisabled,
    kEnabledWithNoTrigger,
    kEnabledWithDefaultTrigger,
  };
  explicit ScopedPrewarmFeatureList(PrewarmState state);
  ScopedPrewarmFeatureList(const ScopedPrewarmFeatureList&) = delete;
  ScopedPrewarmFeatureList& operator=(const ScopedPrewarmFeatureList&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace test

#endif  // CHROME_BROWSER_PRELOADING_SCOPED_PREWARM_FEATURE_LIST_H_
