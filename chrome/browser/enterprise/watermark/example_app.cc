// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/enterprise/watermark/watermark_example.h"
#include "ui/views/examples/examples_main_proc.h"
#include "ui/views/examples/examples_window.h"

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  // The use of base::test::TaskEnvironment in the following function relies on
  // the timeout values from TestTimeouts.
  TestTimeouts::Initialize();

  base::AtExitManager at_exit;

  views::examples::ExampleVector examples;
  examples.push_back(std::make_unique<WatermarkExample>());
  return static_cast<int>(
      views::examples::ExamplesMainProc(false, std::move(examples)));
}
