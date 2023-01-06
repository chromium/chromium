// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/gl/gl_switches.h"

namespace {

int RunHelper(ash::AshTestSuite* test_suite) {
  // In ash_perftests we want to use hardware GL. By adding this switch we can
  // set |use_software_gl| to false in gl_surface_test_support.cc.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kUseGpuInTests);
  return test_suite->Run();
}

}  // namespace

int main(int argc, char** argv) {
  ash::AshTestSuite test_suite(argc, argv);

  mojo::core::Init();
  return base::LaunchUnitTestsSerially(
      argc, argv, base::BindOnce(&RunHelper, base::Unretained(&test_suite)));
}
