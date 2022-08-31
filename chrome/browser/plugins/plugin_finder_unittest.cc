// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_finder.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "content/public/common/webplugininfo.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PluginFinderTest, FindPluginWithIdentifier) {
  std::unique_ptr<PluginMetadata> pdf;
  ASSERT_TRUE(PluginFinder::GetInstance()->FindPluginWithIdentifier(
      "google-chrome-pdf", nullptr, &pdf));
  ASSERT_TRUE(pdf);

  content::WebPluginInfo fake_pdf(u"Chrome PDF Viewer", base::FilePath(), u"0",
                                  std::u16string());
  EXPECT_EQ(PluginMetadata::SECURITY_STATUS_FULLY_TRUSTED,
            pdf->GetSecurityStatus(fake_pdf));
}
