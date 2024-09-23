// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_TEST_UTILS_H_

#include <optional>
#include <string_view>

#include "ash/public/cpp/lobster/lobster_image_candidate.h"
#include "components/manta/proto/manta.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

const SkBitmap CreateTestBitmap(int width, int height);

manta::proto::Request CreateTestMantaRequest(std::string_view query,
                                             std::optional<uint32_t> seed,
                                             const gfx::Size& size,
                                             int num_outputs);

std::unique_ptr<manta::proto::Response> CreateFakeMantaResponse(
    size_t num_candidates,
    const gfx::Size& image_dimensions);

testing::Matcher<ash::LobsterImageCandidate> EqLobsterImageCandidate(
    int expected_id,
    const SkBitmap& expected_bitmap,
    uint32_t expected_generation_seed,
    std::string_view expected_query);

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_TEST_UTILS_H_
