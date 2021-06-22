// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/enhanced_network_tts/enhanced_network_tts_impl.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using EnhancedNetworkTtsImplTest = testing::Test;

TEST_F(EnhancedNetworkTtsImplTest, GetAudioData) {
  std::vector<uint8_t> result;
  const std::string test = "test";
  EnhancedNetworkTtsImpl::GetInstance().GetAudioData(
      test,
      base::BindOnce([](std::vector<uint8_t>* result,
                        const std::vector<uint8_t>& bytes) { *result = bytes; },
                     &result));
  EXPECT_EQ(result[0], 116);
  EXPECT_EQ(result[1], 101);
  EXPECT_EQ(result[2], 115);
  EXPECT_EQ(result[3], 116);
}

}  // namespace ash
