// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_prefs.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_content_client.h"
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
  EXPECT_EQ(PluginPrefs::NO_POLICY,
            plugin_prefs_->PolicyStatusForPlugin(base::ASCIIToUTF16(
                ChromeContentClient::kPDFExtensionPluginName)));

  SetAlwaysOpenPdfExternally(true);

  EXPECT_EQ(PluginPrefs::POLICY_DISABLED,
            plugin_prefs_->PolicyStatusForPlugin(base::ASCIIToUTF16(
                ChromeContentClient::kPDFExtensionPluginName)));
}
