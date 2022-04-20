// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_finder.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "content/public/common/webplugininfo.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PluginFinderTest, BuiltIns) {
  auto* plugin_finder = PluginFinder::GetInstance();

  std::unique_ptr<PluginMetadata> flash;
  ASSERT_TRUE(plugin_finder->FindPluginWithIdentifier("adobe-flash-player",
                                                      nullptr, &flash));
  ASSERT_TRUE(flash);
  EXPECT_EQ(u"Adobe Flash Player", flash->name());
  EXPECT_TRUE(flash->url_for_display());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const auto kPluginUrl = GURL("http://get.adobe.com/flashplayer/");
#else
  const auto kPluginUrl =
      GURL("https://support.google.com/chrome/answer/6258784");
#endif
  EXPECT_EQ(kPluginUrl, flash->plugin_url());
  EXPECT_EQ(GURL("https://support.google.com/chrome/?p=plugin_flash"),
            flash->help_url());
  EXPECT_EQ("en-US", flash->language());
  EXPECT_TRUE(flash->plugin_is_deprecated());
  // Mime types not tested because PluginMetadata::Clone() does not populate
  // them, which means they are not returned out of FindPluginWithIdentifier().
  content::WebPluginInfo fake_flash(u"Adobe Flash Player", base::FilePath(),
                                    u"32.0.0.445", std::u16string());
  EXPECT_EQ(PluginMetadata::SECURITY_STATUS_DEPRECATED,
            flash->GetSecurityStatus(fake_flash));

  std::unique_ptr<PluginMetadata> pdf;
  ASSERT_TRUE(plugin_finder->FindPluginWithIdentifier("google-chrome-pdf",
                                                      nullptr, &pdf));
  ASSERT_TRUE(pdf);
  content::WebPluginInfo fake_pdf(u"Chrome PDF Viewer", base::FilePath(), u"0",
                                  std::u16string());
  EXPECT_EQ(PluginMetadata::SECURITY_STATUS_FULLY_TRUSTED,
            pdf->GetSecurityStatus(fake_pdf));
}
