// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/event_router_forwarder.h"

#include <utility>

#include "base/bind.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/thread_test_helper.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace extensions {

namespace {

const events::HistogramValue kHistogramValue = events::FOR_TEST;
const char kEventName[] = "event_name";

class MockEventRouterForwarder : public EventRouterForwarder {
 public:
  MOCK_METHOD6(CallEventRouter,
               void(Profile*,
                    const std::string&,
                    events::HistogramValue,
                    const std::string&,
                    Profile*,
                    const GURL&));

  void CallEventRouter(Profile* profile,
                       const std::string& extension_id,
                       events::HistogramValue histogram_value,
                       const std::string& event_name,
                       std::unique_ptr<base::ListValue> event_args,
                       Profile* restrict_to_profile,
                       const GURL& event_url) override {
    CallEventRouter(profile, extension_id, histogram_value, event_name,
                    restrict_to_profile, event_url);
  }

 protected:
  ~MockEventRouterForwarder() override {}
};

static void BroadcastEventToRenderers(EventRouterForwarder* event_router,
                                      events::HistogramValue histogram_value,
                                      const std::string& event_name,
                                      const GURL& url) {
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  event_router->BroadcastEventToRenderers(histogram_value, event_name,
                                          std::move(args), url);
}

static void DispatchEventToRenderers(EventRouterForwarder* event_router,
                                     events::HistogramValue histogram_value,
                                     const std::string& event_name,
                                     void* profile,
                                     bool use_profile_to_restrict_events,
                                     const GURL& url) {
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  event_router->DispatchEventToRenderers(histogram_value, event_name,
                                         std::move(args), profile,
                                         use_profile_to_restrict_events, url);
}

}  // namespace

class EventRouterForwarderTest : public testing::Test {
 protected:
  EventRouterForwarderTest()
      : thread_bundle_(content::TestBrowserThreadBundle::REAL_IO_THREAD),
        profile_manager_(
            TestingBrowserProcess::GetGlobal()) {
#if defined(OS_MACOSX)
    base::PowerMonitorDeviceSource::AllocateSystemIOPorts();
#endif
    std::unique_ptr<base::PowerMonitorSource> power_monitor_source(
        new base::PowerMonitorDeviceSource());
    dummy.reset(new base::PowerMonitor(std::move(power_monitor_source)));
  }

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    // Inject a BrowserProcess with a ProfileManager.
    profile1_ = profile_manager_.CreateTestingProfile("one");
    profile2_ = profile_manager_.CreateTestingProfile("two");
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<base::PowerMonitor> dummy;
  // Profiles are weak pointers, owned by ProfileManager in |browser_process_|.
  TestingProfile* profile1_;
  TestingProfile* profile2_;
};

TEST_F(EventRouterForwarderTest, BroadcastRendererUI) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  GURL url;
  EXPECT_CALL(*event_router, CallEventRouter(profile1_, "", kHistogramValue,
                                             kEventName, profile1_, url));
  EXPECT_CALL(*event_router, CallEventRouter(profile2_, "", kHistogramValue,
                                             kEventName, profile2_, url));
  BroadcastEventToRenderers(event_router.get(), kHistogramValue, kEventName,
                            url);
}

TEST_F(EventRouterForwarderTest, BroadcastRendererUIIncognito) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  using ::testing::_;
  GURL url;
  Profile* incognito = profile1_->GetOffTheRecordProfile();
  EXPECT_CALL(*event_router, CallEventRouter(profile1_, "", kHistogramValue,
                                             kEventName, profile1_, url));
  EXPECT_CALL(*event_router, CallEventRouter(incognito, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*event_router, CallEventRouter(profile2_, "", kHistogramValue,
                                             kEventName, profile2_, url));
  BroadcastEventToRenderers(event_router.get(), kHistogramValue, kEventName,
                            url);
}

// This is the canonical test for passing control flow from the IO thread
// to the UI thread. Repeating this for all public functions of
// EventRouterForwarder would not increase coverage.
TEST_F(EventRouterForwarderTest, BroadcastRendererIO) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  GURL url;
  EXPECT_CALL(*event_router, CallEventRouter(profile1_, "", kHistogramValue,
                                             kEventName, profile1_, url));
  EXPECT_CALL(*event_router, CallEventRouter(profile2_, "", kHistogramValue,
                                             kEventName, profile2_, url));
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(&BroadcastEventToRenderers,
                                          base::Unretained(event_router.get()),
                                          kHistogramValue, kEventName, url));

  // Wait for IO thread's message loop to be processed
  scoped_refptr<base::ThreadTestHelper> helper(new base::ThreadTestHelper(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}).get()));
  ASSERT_TRUE(helper->Run());

  base::RunLoop().RunUntilIdle();
}

TEST_F(EventRouterForwarderTest, UnicastRendererUIRestricted) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  using ::testing::_;
  GURL url;
  EXPECT_CALL(*event_router, CallEventRouter(profile1_, "", kHistogramValue,
                                             kEventName, profile1_, url));
  EXPECT_CALL(*event_router, CallEventRouter(profile2_, _, _, _, _, _))
      .Times(0);
  DispatchEventToRenderers(event_router.get(), kHistogramValue, kEventName,
                           profile1_, true, url);
}

TEST_F(EventRouterForwarderTest, UnicastRendererUIRestrictedIncognito1) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  Profile* incognito = profile1_->GetOffTheRecordProfile();
  using ::testing::_;
  GURL url;
  EXPECT_CALL(*event_router, CallEventRouter(profile1_, "", kHistogramValue,
                                             kEventName, profile1_, url));
  EXPECT_CALL(*event_router, CallEventRouter(incognito, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*event_router, CallEventRouter(profile2_, _, _, _, _, _))
      .Times(0);
  DispatchEventToRenderers(event_router.get(), kHistogramValue, kEventName,
                           profile1_, true, url);
}

TEST_F(EventRouterForwarderTest, UnicastRendererUIRestrictedIncognito2) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  Profile* incognito = profile1_->GetOffTheRecordProfile();
  using ::testing::_;
  GURL url;
  EXPECT_CALL(*event_router, CallEventRouter(profile1_, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*event_router, CallEventRouter(incognito, "", kHistogramValue,
                                             kEventName, incognito, url));
  EXPECT_CALL(*event_router, CallEventRouter(profile2_, _, _, _, _, _))
      .Times(0);
  DispatchEventToRenderers(event_router.get(), kHistogramValue, kEventName,
                           incognito, true, url);
}

TEST_F(EventRouterForwarderTest, UnicastRendererUIUnrestricted) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  using ::testing::_;
  GURL url;
  EXPECT_CALL(*event_router, CallEventRouter(profile1_, "", kHistogramValue,
                                             kEventName, NULL, url));
  EXPECT_CALL(*event_router, CallEventRouter(profile2_, _, _, _, _, _))
      .Times(0);
  DispatchEventToRenderers(event_router.get(), kHistogramValue, kEventName,
                           profile1_, false, url);
}

TEST_F(EventRouterForwarderTest, UnicastRendererUIUnrestrictedIncognito) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  Profile* incognito = profile1_->GetOffTheRecordProfile();
  using ::testing::_;
  GURL url;
  EXPECT_CALL(*event_router, CallEventRouter(profile1_, "", kHistogramValue,
                                             kEventName, NULL, url));
  EXPECT_CALL(*event_router, CallEventRouter(incognito, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*event_router, CallEventRouter(profile2_, _, _, _, _, _))
      .Times(0);
  DispatchEventToRenderers(event_router.get(), kHistogramValue, kEventName,
                           profile1_, false, url);
}

}  // namespace extensions
