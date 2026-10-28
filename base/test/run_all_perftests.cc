// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/allow_check_is_test_for_testing.h"
#include "base/test/perf_test_suite.h"

int main(int argc, char** argv) {
  base::test::AllowCheckIsTestForTesting();
  return base::PerfTestSuite(argc, argv).Run();
}
