// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// An empty test so that the test runner has something to run.
TEST(EmptyTest, Empty) {}

}  // namespace

// TODO(https://crbug.com/942564): Remove this test suite from the bots and then
// delete it. For now we run a stub suite so we can do the removal in a separate
// CL.
int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
