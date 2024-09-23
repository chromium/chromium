// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/com_init_util.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/process/set_process_title_linux.h"
#endif

namespace base {

namespace {

#if BUILDFLAG(IS_WIN)
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
#endif  // BUILDFLAG(IS_WIN)

class BaseUnittestSuite : public TestSuite {
 public:
  BaseUnittestSuite(int argc, char** argv) : TestSuite(argc, argv) {}

 protected:
  void Initialize() override {
    TestSuite::Initialize();

#if BUILDFLAG(IS_WIN)
    // Add TestEventListeners to enforce certain properties across tests.
    testing::TestEventListeners& listeners =
        testing::UnitTest::GetInstance()->listeners();
    listeners.Append(new ComLeakCheck);
    listeners.Append(new HistogramAllocatorCheck);
    listeners.Append(new TimerCheck);
#endif  // BUILDFLAG(IS_WIN)
  }
};

}  // namespace

}  // namespace base

int main(int argc, char** argv) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // For setproctitle unit tests.
  setproctitle_init(const_cast<const char**>(argv));
#endif

  base::BaseUnittestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
