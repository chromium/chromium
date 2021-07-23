// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/networking_log.h"

#include "ash/webui/diagnostics_ui/backend/log_test_helpers.h"
#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace diagnostics {

class NetworkingLogTest : public testing::Test {
 public:
  NetworkingLogTest() = default;

  ~NetworkingLogTest() override = default;
};

TEST_F(NetworkingLogTest, LogHasHeader) {
  NetworkingLog log;
  const std::string log_as_string = log.GetContents();
  const std::vector<std::string> log_lines = GetLogLines(log_as_string);

  // Expect one title line.
  EXPECT_EQ(1u, log_lines.size());
  EXPECT_EQ("--- Networking Info ---", log_lines[0]);
}

}  // namespace diagnostics
}  // namespace ash
