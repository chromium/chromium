// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/allow_check_is_test_for_testing.h"
#include "base/test/test_suite.h"
#include "content/public/common/content_switches.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace {

class AndroidWebViewTestSuite : public base::TestSuite {
 public:
  AndroidWebViewTestSuite(int argc, char** argv)
      : base::TestSuite(argc, argv) {}
  AndroidWebViewTestSuite(const AndroidWebViewTestSuite&) = delete;
  AndroidWebViewTestSuite& operator=(const AndroidWebViewTestSuite&) = delete;

 private:
  void Initialize() override {
    base::TestSuite::Initialize();

    ui::RegisterPathProvider();
    base::FilePath pak_path;
    ASSERT_TRUE(
        base::PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(
        pak_path.AppendASCII("android_webview_unittests_resources.pak"));
  }

  void Shutdown() override {
    ui::ResourceBundle::CleanupSharedInstance();
    base::TestSuite::Shutdown();
  }
};

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kSingleProcess);
  command_line->AppendSwitchASCII(switches::kDisableFeatures, ",Vulkan");
  command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                  ",WebViewNewInvalidateHeuristic");

  gl::GLSurfaceTestSupport::InitializeNoExtensionsOneOff();
  base::test::AllowCheckIsTestForTesting();
  AndroidWebViewTestSuite test_suite(argc, argv);
  mojo::core::Init();
  return test_suite.Run();
}
