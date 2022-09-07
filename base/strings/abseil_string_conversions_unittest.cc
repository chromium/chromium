// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/abseil_string_conversions.h"

#include <vector>

#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_piece_forward.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"

namespace base {

namespace {
using string_view = absl::string_view;
}  // namespace

TEST(AbseilStringConversionsTest, StringPieceToStringView) {
  static constexpr StringPiece kPiece = "foo";
  static constexpr string_view kView = StringPieceToStringView(kPiece);
  static_assert(kPiece.data() == kView.data(), "");
  static_assert(kPiece.size() == kView.size(), "");
}

TEST(AbseilStringConversionsTest, StringViewToStringPiece) {
  static constexpr string_view kView = "bar";
  static constexpr StringPiece kPiece = StringViewToStringPiece(kView);
  static_assert(kView.data() == kPiece.data(), "");
  static_assert(kView.size() == kPiece.size(), "");
}

TEST(AbseilStringConversionsTest, StringPiecesToStringViews) {
  static constexpr StringPiece kFoo = "foo";
  static constexpr StringPiece kBar = "bar";
  static constexpr StringPiece kBaz = "baz";

  const std::vector<StringPiece> kPieces = {kFoo, kBar, kBaz};
  const std::vector<string_view> kViews = StringPiecesToStringViews(kPieces);

  ASSERT_EQ(kViews.size(), 3u);
  EXPECT_EQ(kViews[0].data(), kFoo);
  EXPECT_EQ(kViews[0].size(), 3u);
  EXPECT_EQ(kViews[1].data(), kBar);
  EXPECT_EQ(kViews[1].size(), 3u);
  EXPECT_EQ(kViews[2].data(), kBaz);
  EXPECT_EQ(kViews[2].size(), 3u);
}

TEST(AbseilStringConversionsTest, StringViewsToStringPieces) {
  static constexpr string_view kFoo = "foo";
  static constexpr string_view kBar = "bar";
  static constexpr string_view kBaz = "baz";

  const std::vector<string_view> kViews = {kFoo, kBar, kBaz};
  const std::vector<StringPiece> kPieces = StringViewsToStringPieces(kViews);

  ASSERT_EQ(kPieces.size(), 3u);
  EXPECT_EQ(kPieces[0].data(), kFoo);
  EXPECT_EQ(kPieces[0].size(), 3u);
  EXPECT_EQ(kPieces[1].data(), kBar);
  EXPECT_EQ(kPieces[1].size(), 3u);
  EXPECT_EQ(kPieces[2].data(), kBaz);
  EXPECT_EQ(kPieces[2].size(), 3u);
}

}  // namespace base
