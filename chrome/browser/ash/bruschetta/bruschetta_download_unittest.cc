// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_download.h"

#include <array>
#include <cstring>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bruschetta {
namespace {

class BruschettaDownloadTest : public testing::Test {};

TEST_F(BruschettaDownloadTest, TestSha256File) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const int kBufSize =
      4096 * 2 + 1;  // Sha256File uses 4k block size, so make sure the file is
                     // multiple blocks and not a complete block.
  std::array<uint8_t, kBufSize> buff;
  buff.fill('d');
  auto path = dir.GetPath().Append("file");
  const char* expected =
      "B1AAAD3DBE85816D70C94C35B873D45F0C68F9D3B3DB6F6AB858A1560540E4DF";
  ASSERT_TRUE(base::WriteFile(path, buff));
  auto hash = Sha256FileForTesting(path);

  ASSERT_EQ(hash, expected);
}

}  // namespace
}  // namespace bruschetta
