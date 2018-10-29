// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "chrome/browser/android/vr/test/ar_test_suite.h"

int main(int argc, char** argv) {
  vr::ArTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindRepeating(&vr::ArTestSuite::Run,
                          base::Unretained(&test_suite)));
}
