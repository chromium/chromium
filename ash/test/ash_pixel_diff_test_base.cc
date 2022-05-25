// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_pixel_diff_test_base.h"

#include "base/command_line.h"
#include "ui/compositor/compositor_switches.h"

namespace ash {

AshPixelDiffTestBase::AshPixelDiffTestBase(
    std::unique_ptr<base::test::TaskEnvironment> task_environment)
    : AshTestBase(std::move(task_environment)) {}

AshPixelDiffTestBase::~AshPixelDiffTestBase() = default;

void AshPixelDiffTestBase::SetUp() {
  // In ash_pixeltests, we want to take screenshots then compare them with the
  // benchmark images. Therefore, enable pixel output in tests.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnablePixelOutputInTests);

  AshTestBase::SetUp();
}

}  // namespace ash
