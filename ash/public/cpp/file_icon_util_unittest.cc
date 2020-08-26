// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/file_icon_util.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(FileIconUtilTest, GetIconTypeForPath) {
  const std::vector<std::pair<std::string, internal::IconType>>
      file_path_to_icon_type = {
          {"/my/test/file.pdf", internal::IconType::kPdf},
          {"/my/test/file.Pdf", internal::IconType::kPdf},
          {"/my/test/file.tar.gz", internal::IconType::kArchive},
          {"/my/test/.gslides", internal::IconType::kGslide},
          {"/my/test/noextension", internal::IconType::kGeneric},
          {"/my/test/file.missing", internal::IconType::kGeneric}};

  for (const auto& pair : file_path_to_icon_type) {
    EXPECT_EQ(internal::GetIconTypeForPath(base::FilePath(pair.first)),
              pair.second);
  }
}

}  // namespace ash
