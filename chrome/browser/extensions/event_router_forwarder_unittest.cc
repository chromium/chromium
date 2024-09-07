// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/event_router_forwarder.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/thread_test_helper.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

namespace {

const events::HistogramValue kHistogramValue = events::FOR_TEST;
const char kEventName[] = "event_name";

class MockEventRouterForwarder : public EventRouterForwarder {
 public:
  MOCK_METHOD3(CallEventRouter,
               void(Profile*, events::HistogramValue, const std::string&));

  void CallEventRouter(Profile* profile,
                       events::HistogramValue histogram_value,
                       const std::string& event_name,
                       base::Value::List args) override {
    CallEventRouter(profile, histogram_value, event_name);
  }

 protected:
  ~MockEventRouterForwarder() override {}
};

static void BroadcastEventToRenderers(
    EventRouterForwarder* event_router,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    bool dispatch_to_off_the_record_profiles) {
  event_router->BroadcastEventToRenderers(histogram_value, event_name,
                                          base::Value::List(),
                                          dispatch_to_off_the_record_profiles);
}

}  // namespace

class EventRouterForwarderTest : public testing::Test {
 protected:
  EventRouterForwarderTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {
  }

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    // Inject a BrowserProcess with a ProfileManager.
    profile1_ = profile_manager_.CreateTestingProfile("one");
    profile2_ = profile_manager_.CreateTestingProfile("two");
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  // Profiles are weak pointers, owned by ProfileManager in |browser_process_|.
  raw_ptr<TestingProfile> profile1_;
  raw_ptr<TestingProfile> profile2_;
};

TEST_F(EventRouterForwarderTest, BroadcastRendererUI) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  GURL url;
  EXPECT_CALL(*event_router,
              CallEventRouter(profile1_.get(), kHistogramValue, kEventName));
  EXPECT_CALL(*event_router,
              CallEventRouter(profile2_.get(), kHistogramValue, kEventName));
  BroadcastEventToRenderers(event_router.get(), kHistogramValue, kEventName,
                            false);
}

TEST_F(EventRouterForwarderTest, BroadcastRendererUIIncognito) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  using ::testing::_;
  GURL url;
  Profile* incognito =
      profile1_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_CALL(*event_router,
              CallEventRouter(profile1_.get(), kHistogramValue, kEventName));
  EXPECT_CALL(*event_router, CallEventRouter(incognito, _, _)).Times(0);
  EXPECT_CALL(*event_router,
              CallEventRouter(profile2_.get(), kHistogramValue, kEventName));
  BroadcastEventToRenderers(event_router.get(), kHistogramValue, kEventName,
                            false);
}

TEST_F(EventRouterForwarderTest,
       BroadcastRendererUIIncognitoWithDispatchToOffTheRecordProfiles) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  using ::testing::_;
  GURL url;
  Profile* incognito1 =
      profile1_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  Profile* incognito2 =
      profile2_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_CALL(*event_router,
              CallEventRouter(profile1_.get(), kHistogramValue, kEventName));
  EXPECT_CALL(*event_router, CallEventRouter(incognito1, _, _));
  EXPECT_CALL(*event_router,
              CallEventRouter(profile2_.get(), kHistogramValue, kEventName));
  EXPECT_CALL(*event_router, CallEventRouter(incognito2, _, _));
  BroadcastEventToRenderers(event_router.get(), kHistogramValue, kEventName,
                            true);
}

// This is the canonical test for passing control flow from the IO thread
// to the UI thread. Repeating this for all public functions of
// EventRouterForwarder would not increase coverage.
TEST_F(EventRouterForwarderTest, BroadcastRendererIO) {
  scoped_refptr<MockEventRouterForwarder> event_router(
      new MockEventRouterForwarder);
  GURL url;
  EXPECT_CALL(*event_router,
              CallEventRouter(profile1_.get(), kHistogramValue, kEventName));
  EXPECT_CALL(*event_router,
              CallEventRouter(profile2_.get(), kHistogramValue, kEventName));
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BroadcastEventToRenderers,
                                base::Unretained(event_router.get()),
                                kHistogramValue, kEventName, false));

  // Wait for IO thread's message loop to be processed
  scoped_refptr<base::ThreadTestHelper> helper(
      new base::ThreadTestHelper(content::GetIOThreadTaskRunner({}).get()));
  ASSERT_TRUE(helper->Run());

  base::RunLoop().RunUntilIdle();
}

}  // namespace extensions
