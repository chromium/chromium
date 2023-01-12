// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_prefs.h"

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/common/chrome_content_client.h"
#include "content/public/common/webplugininfo.h"
#include "testing/gtest/include/gtest/gtest.h"

class PluginPrefsTest : public ::testing::Test {
 public:
  void SetUp() override { plugin_prefs_ = new PluginPrefs(); }

  void SetAlwaysOpenPdfExternally(bool value) {
    plugin_prefs_->SetAlwaysOpenPdfExternallyForTests(value);
  }

 protected:
  scoped_refptr<PluginPrefs> plugin_prefs_;
};

TEST_F(PluginPrefsTest, AlwaysOpenPdfExternally) {
  content::WebPluginInfo pdf_plugin_info;
  pdf_plugin_info.path =
      base::FilePath(ChromeContentClient::kPDFExtensionPluginPath);

  EXPECT_TRUE(plugin_prefs_->IsPluginEnabled(pdf_plugin_info));

  SetAlwaysOpenPdfExternally(true);

  EXPECT_FALSE(plugin_prefs_->IsPluginEnabled(pdf_plugin_info));
}
