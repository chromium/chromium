// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_SCOPED_BROWSER_CONTROLS_LINEAR_ANIMATION_H_
#define CC_TEST_SCOPED_BROWSER_CONTROLS_LINEAR_ANIMATION_H_

#include "base/test/scoped_feature_list.h"
#include "cc/base/features.h"

namespace cc::test {

// Scoped class to disable the browser controls scroll snap animation in
// tests that control the position of the browser controls by scrolling the
// page.
class ScopedBrowserControlsLinearAnimation {
 public:
  ScopedBrowserControlsLinearAnimation() = default;
  ~ScopedBrowserControlsLinearAnimation() = default;

 private:
  base::test::ScopedFeatureList feature_list_{
      {},
      {features::kBrowserControlsScrollSnapAnimation}};
};

}  // namespace cc::test

#endif  // CC_TEST_SCOPED_BROWSER_CONTROLS_LINEAR_ANIMATION_H_
