// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/unit_test_launcher.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "mojo/core/embedder/embedder.h"

int main(int argc, char** argv) {
  ash::AshTestSuite test_suite(argc, argv);

  mojo::core::Init();
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&ash::AshTestSuite::Run, base::Unretained(&test_suite)));
}
