// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/ash_webui_test_suite.h"
#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/chromeos_buildflags.h"
#include "mojo/core/embedder/embedder.h"

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
#error This test target only builds with linux-chromeos, not for real ChromeOS\
 devices. See comment in build/config/chromeos/args.gni.
#endif

int main(int argc, char** argv) {
  AshWebUITestSuite test_suite(argc, argv);

  // Some tests use mojo
  mojo::core::Init();
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&AshWebUITestSuite::Run, base::Unretained(&test_suite)));
}
