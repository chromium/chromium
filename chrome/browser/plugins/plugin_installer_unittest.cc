// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_installer.h"

#include <memory>

#include "chrome/browser/plugins/plugin_installer_observer.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace {

class PluginInstallerTest : public ChromeRenderViewHostTestHarness {
 public:
  PluginInstallerTest();
  void SetUp() override;
  void TearDown() override;

  PluginInstaller* installer() { return installer_.get(); }

 private:
  std::unique_ptr<PluginInstaller> installer_;
};

PluginInstallerTest::PluginInstallerTest() {
}

void PluginInstallerTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();
  installer_ = std::make_unique<PluginInstaller>();
}

void PluginInstallerTest::TearDown() {
  installer_.reset();
  content::RenderViewHostTestHarness::TearDown();
}

class TestPluginInstallerObserver : public PluginInstallerObserver {
 public:
  explicit TestPluginInstallerObserver(PluginInstaller* installer)
      : PluginInstallerObserver(installer), download_finished_(false) {}

  bool download_finished() const { return download_finished_; }

 private:
  void DownloadFinished() override { download_finished_ = true; }

  bool download_finished_;
};

const char kTestUrl[] = "http://example.com/some-url";

}  // namespace

// Test that the DownloadFinished() notifications is sent to observers when the
// PluginInstaller opens a download URL.
TEST_F(PluginInstallerTest, OpenDownloadURL) {
  TestPluginInstallerObserver installer_observer(installer());
  installer()->OpenDownloadURL(GURL(kTestUrl), web_contents());

  EXPECT_TRUE(installer_observer.download_finished());
}
