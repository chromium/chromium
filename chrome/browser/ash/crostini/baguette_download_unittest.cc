// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/baguette_download.h"

#include <array>
#include <cstring>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

namespace {

class BaguetteDownloadTest : public testing::Test {};

TEST_F(BaguetteDownloadTest, TestSha256File) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  // Sha256File uses 4k block size, so make sure the file is multiple blocks and
  // not a complete block.
  const int bufSize = 4096 * 2 + 1;
  std::array<uint8_t, bufSize> buf;
  buf.fill('d');
  auto path = dir.GetPath().Append("file");
  const char* expected =
      "B1AAAD3DBE85816D70C94C35B873D45F0C68F9D3B3DB6F6AB858A1560540E4DF";
  ASSERT_TRUE(base::WriteFile(path, buf));
  auto hash = Sha256FileForTesting(path);

  ASSERT_EQ(hash, expected);
}

}  // namespace

}  // namespace crostini
