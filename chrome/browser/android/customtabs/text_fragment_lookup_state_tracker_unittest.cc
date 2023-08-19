// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/customtabs/text_fragment_lookup_state_tracker.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace customtabs {

class TextFragmentLookupStateTrackerTest : public testing::Test {
 public:
  TextFragmentLookupStateTrackerTest() = default;
};

TEST_F(TextFragmentLookupStateTrackerTest, ExtractAllowedTextDirectives) {
  TextFragmentLookupStateTracker state_tracker(nullptr);
  std::vector<std::string> allowed_texts;
  state_tracker.lookup_count_ = 43;
  allowed_texts = state_tracker.ExtractAllowedTextDirectives({"ab", "cd"});
  EXPECT_THAT(allowed_texts.size(), 2);

  state_tracker.lookup_count_ = 45;
  allowed_texts = state_tracker.ExtractAllowedTextDirectives({"ab", "cd"});
  EXPECT_THAT(allowed_texts.size(), 0);
}

}  // namespace customtabs
