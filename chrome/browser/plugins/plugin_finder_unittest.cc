// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_finder.h"

#include <memory>
#include <string>

#include "chrome/browser/plugins/plugin_metadata.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PluginFinderTest, FindPluginWithIdentifier) {
  std::unique_ptr<PluginMetadata> pdf;
  ASSERT_TRUE(PluginFinder::GetInstance()->FindPluginWithIdentifier(
      "google-chrome-pdf", nullptr, &pdf));
  ASSERT_TRUE(pdf);

  EXPECT_EQ(PluginMetadata::SECURITY_STATUS_FULLY_TRUSTED,
            pdf->security_status());
}
