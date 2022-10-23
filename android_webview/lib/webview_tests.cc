// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/test/allow_check_is_test_for_testing.h"
#include "base/test/test_suite.h"
#include "content/public/common/content_switches.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/test/gl_surface_test_support.h"

int main(int argc, char** argv) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kSingleProcess);
  command_line->AppendSwitchASCII(switches::kDisableFeatures, ",Vulkan");
  command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                  ",WebViewNewInvalidateHeuristic");

  gl::GLSurfaceTestSupport::InitializeNoExtensionsOneOff();
  base::test::AllowCheckIsTestForTesting();
  base::TestSuite test_suite(argc, argv);
  mojo::core::Init();
  return test_suite.Run();
}
