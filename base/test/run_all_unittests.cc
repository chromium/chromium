// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include "base/win/com_init_util.h"
#endif  // defined(OS_WIN)

namespace base {

namespace {

#if defined(OS_WIN)
class ComLeakCheck : public testing::EmptyTestEventListener {
 public:
  void OnTestEnd(const testing::TestInfo& test) override {
    // Verify that COM has been reset to defaults by the test.
    EXPECT_EQ(win::GetComApartmentTypeForThread(), win::ComApartmentType::NONE);
  }
};

class HistogramAllocatorCheck : public testing::EmptyTestEventListener {
 public:
  void OnTestEnd(const testing::TestInfo& test) override {
    // Verify that the histogram allocator was released by the test.
    CHECK(!GlobalHistogramAllocator::Get());
  }
};

class TimerCheck : public testing::EmptyTestEventListener {
 public:
  void OnTestEnd(const testing::TestInfo& test_info) override {
    EXPECT_FALSE(Time::IsHighResolutionTimerInUse());
  }
};
#endif  // defined(OS_WIN)

class BaseUnittestSuite : public TestSuite {
 public:
  BaseUnittestSuite(int argc, char** argv) : TestSuite(argc, argv) {}

 protected:
  void Initialize() override {
    TestSuite::Initialize();

#if defined(OS_WIN)
    // Add TestEventListeners to enforce certain properties across tests.
    testing::TestEventListeners& listeners =
        testing::UnitTest::GetInstance()->listeners();
    listeners.Append(new ComLeakCheck);
    listeners.Append(new HistogramAllocatorCheck);
    listeners.Append(new TimerCheck);
#endif  // defined(OS_WIN)
  }
};

}  // namespace

}  // namespace base

int main(int argc, char** argv) {
  base::BaseUnittestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
