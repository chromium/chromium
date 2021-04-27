// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if defined(OS_WIN)
class TimerCheck : public testing::EmptyTestEventListener {
 public:
  void OnTestEnd(const testing::TestInfo& test_info) override {
    EXPECT_FALSE(base::Time::IsHighResolutionTimerInUse());
  }
};
#endif

class BaseUnittestSuite : public base::TestSuite {
 public:
  BaseUnittestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();

#if defined(OS_WIN)
    // Add TestEventListeners to enforce certain properties across tests.
    testing::TestEventListeners& listeners =
        testing::UnitTest::GetInstance()->listeners();
    listeners.Append(new TimerCheck);
#endif
  }
};

int main(int argc, char** argv) {
  BaseUnittestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
