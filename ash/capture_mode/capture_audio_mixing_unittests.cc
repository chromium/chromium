// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class CaptureAudioMixingTest : public AshTestBase {
 public:
  CaptureAudioMixingTest()
      : scoped_feature_list_(features::kCaptureModeAudioMixing) {}
  CaptureAudioMixingTest(const CaptureAudioMixingTest&) = delete;
  CaptureAudioMixingTest& operator=(const CaptureAudioMixingTest&) = delete;
  ~CaptureAudioMixingTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CaptureAudioMixingTest, Basic) {
  EXPECT_TRUE(features::IsCaptureModeAudioMixingEnabled());
}

}  // namespace ash
