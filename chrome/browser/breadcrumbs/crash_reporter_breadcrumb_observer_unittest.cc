// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/crash_reporter_breadcrumb_observer.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#include "components/breadcrumbs/core/crash_reporter_breadcrumb_constants.h"
#include "components/crash/core/common/crash_key.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// Returns the number of times that |substring| appears in |string|. Overlapping
// instances of |substring| are not counted.
int CountSubstrings(const std::string& string, const std::string& substring) {
  const size_t substring_length = substring.length();
  int count = 0;
  size_t pos = 0;
  while ((pos = string.find(substring, pos)) != std::string::npos) {
    pos += substring_length;
    count++;
  }
  return count;
}

// Returns the breadcrumbs string currently stored by the crash reporter.
std::string GetBreadcrumbsCrashKeyValue() {
  return crash_reporter::GetCrashKeyValue(
      breadcrumbs::kBreadcrumbsProductDataKey);
}

}  // namespace

// Tests that CrashReporterBreadcrumbObserver attaches observed breadcrumb
// events to crash reports.
class CrashReporterBreadcrumbObserverTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    // Ensure the CrashReporterBreadcrumbObserver singleton is created
    // and registered.
    breadcrumbs::CrashReporterBreadcrumbObserver::GetInstance();

    crash_reporter::InitializeCrashKeysForTesting();
  }

  void TearDown() override {
    // TODO(crbug.com/40205024) This should call
    // crash_reporter::ResetCrashKeysForTesting() once
    // UserManagerImpl::UpdateNumberOfUsers allows the static
    // local crash_key to be cleared between tests.
    PlatformTest::TearDown();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile browser_context_;
  TestingProfile browser_context_2_;
};

// Tests that breadcrumb events logged to a single BreadcrumbManagerKeyedService
// are collected by the CrashReporterBreadcrumbObserver and attached to crash
// reports.
TEST_F(CrashReporterBreadcrumbObserverTest, EventsAttachedToCrashReport) {
  BreadcrumbManagerKeyedServiceFactory::GetForBrowserContext(&browser_context_)
      ->AddEvent(std::string("Breadcrumb Event"));

  const auto& events =
      breadcrumbs::BreadcrumbManager::GetInstance().GetEvents();
  std::string expected_breadcrumbs;
  for (const auto& event : events)
    expected_breadcrumbs += event + "\n";

  EXPECT_EQ(expected_breadcrumbs, GetBreadcrumbsCrashKeyValue());
}

// TODO(crbug.com/40199927): re-enable the test once this Breakpad bug is fixed.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ProductDataOverflow DISABLED_ProductDataOverflow
#else
#define MAYBE_ProductDataOverflow ProductDataOverflow
#endif

// Tests that breadcrumbs string is cut when it exceeds the max allowed length.
TEST_F(CrashReporterBreadcrumbObserverTest, MAYBE_ProductDataOverflow) {
  // Build a sample breadcrumbs string greater than the maximum allowed size.
  std::string breadcrumbs;
  while (breadcrumbs.length() < breadcrumbs::kMaxDataLength) {
    breadcrumbs.append("12:01 Fake Breadcrumb Event/n");
  }
  breadcrumbs.append("12:01 Fake Breadcrumb Event/n");
  ASSERT_GT(breadcrumbs.length(), breadcrumbs::kMaxDataLength);

  BreadcrumbManagerKeyedServiceFactory::GetForBrowserContext(&browser_context_)
      ->AddEvent(breadcrumbs);

  // Confirm that the total length of the breadcrumbs crash string is
  // |breadcrumbs::kMaxDataLength|.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Linux uses Breakpad, which breaks the crash key value up into chunks of 127
  // characters each, named <crash key>__1, <crash key>__2, etc. These must be
  // summed to determine the total length of the breadcrumbs crash string.
  int chunk = 1;
  size_t chunk_length = 0;
  size_t breadcrumbs_crash_string_length = 0;
  do {
    chunk_length =
        crash_reporter::GetCrashKeyValue(
            base::StringPrintf("%s__%d",
                               breadcrumbs::kBreadcrumbsProductDataKey, chunk))
            .length();
    breadcrumbs_crash_string_length += chunk_length;
    chunk++;
  } while (chunk_length > 0);

  EXPECT_EQ(breadcrumbs::kMaxDataLength, breadcrumbs_crash_string_length);
#else
  EXPECT_EQ(breadcrumbs::kMaxDataLength,
            GetBreadcrumbsCrashKeyValue().length());
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}

// Tests that breadcrumb events logged to multiple BreadcrumbManagerKeyedService
// instances are collected by the CrashReporterBreadcrumbObserver and attached
// to crash reports.
TEST_F(CrashReporterBreadcrumbObserverTest,
       MultipleBrowserContextsAttachedToCrashReport) {
  const std::string event = "Breadcrumb Event";

  breadcrumbs::BreadcrumbManagerKeyedService* breadcrumb_service =
      BreadcrumbManagerKeyedServiceFactory::GetForBrowserContext(
          &browser_context_);
  breadcrumb_service->AddEvent(event);
  EXPECT_EQ(1, CountSubstrings(GetBreadcrumbsCrashKeyValue(), event));

  breadcrumbs::BreadcrumbManagerKeyedService* otr_breadcrumb_service =
      BreadcrumbManagerKeyedServiceFactory::GetForBrowserContext(
          browser_context_.GetOffTheRecordProfile(
              Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true));
  otr_breadcrumb_service->AddEvent(event);
  EXPECT_EQ(2, CountSubstrings(GetBreadcrumbsCrashKeyValue(), event));

  breadcrumbs::BreadcrumbManagerKeyedService* breadcrumb_service_2 =
      BreadcrumbManagerKeyedServiceFactory::GetForBrowserContext(
          &browser_context_2_);
  breadcrumb_service_2->AddEvent(event);
  EXPECT_EQ(3, CountSubstrings(GetBreadcrumbsCrashKeyValue(), event));
}
