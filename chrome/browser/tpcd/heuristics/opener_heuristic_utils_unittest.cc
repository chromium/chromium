// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/heuristics/opener_heuristic_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(OpenerHeuristicUtilsTest, GetPopupProvider) {
  // Any google.com subdomain.
  EXPECT_EQ(GetPopupProvider(GURL("https://accounts.google.com/")),
            PopupProvider::kGoogle);
  EXPECT_EQ(GetPopupProvider(GURL("https://www.google.com/")),
            PopupProvider::kGoogle);
  // Also match http (just in case).
  EXPECT_EQ(GetPopupProvider(GURL("http://www.google.com/")),
            PopupProvider::kGoogle);

  // If not a known provider, return kUnknown.
  EXPECT_EQ(GetPopupProvider(GURL("https://www.example.com/")),
            PopupProvider::kUnknown);
}
