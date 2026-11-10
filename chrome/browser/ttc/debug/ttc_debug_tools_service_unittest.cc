// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/debug/ttc_debug_tools_service.h"

#include <memory>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/debug/ttc_debug_session.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {
namespace {

class TtcDebugToolsServiceTest : public testing::Test {
 protected:
  TtcDebugToolsServiceTest() {
    service_.SetTickClockForTesting(task_environment_.GetMockTickClock());
  }

  ~TtcDebugToolsServiceTest() override { service_.Shutdown(); }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TtcDebugToolsService service_;
};

TEST_F(TtcDebugToolsServiceTest, NoSessionInitially) {
  EXPECT_FALSE(service_.current_session());
}

TEST_F(TtcDebugToolsServiceTest, BeginSessionRecords) {
  TtcDebugSession* session = service_.BeginSession(/*journal=*/nullptr);
  ASSERT_TRUE(session);
  EXPECT_EQ(service_.current_session(), session);
  EXPECT_FALSE(session->ended());
}

TEST_F(TtcDebugToolsServiceTest, NewSessionReplacesPrevious) {
  int changes = 0;
  base::CallbackListSubscription subscription =
      service_.AddSessionChangedCallback(
          base::BindRepeating([](int* changes) { ++*changes; }, &changes));
  base::WeakPtr<TtcDebugSession> first =
      service_.BeginSession(nullptr)->GetWeakPtr();
  EXPECT_EQ(changes, 1);
  TtcDebugSession* second = service_.BeginSession(nullptr);
  EXPECT_EQ(changes, 2);
  EXPECT_FALSE(first);
  EXPECT_EQ(service_.current_session(), second);
}

TEST_F(TtcDebugToolsServiceTest, EndedSessionStaysAvailable) {
  int changes = 0;
  base::CallbackListSubscription subscription =
      service_.AddSessionChangedCallback(
          base::BindRepeating([](int* changes) { ++*changes; }, &changes));
  TtcDebugSession* session = service_.BeginSession(nullptr);
  service_.EndSession(session);
  EXPECT_EQ(changes, 2);
  ASSERT_EQ(service_.current_session(), session);
  EXPECT_TRUE(session->ended());

  // Ending a session that is no longer current does nothing.
  service_.EndSession(nullptr);
  EXPECT_EQ(changes, 2);
}

TEST_F(TtcDebugToolsServiceTest, ShutdownDropsTheRecord) {
  base::WeakPtr<TtcDebugSession> session =
      service_.BeginSession(nullptr)->GetWeakPtr();
  service_.Shutdown();
  EXPECT_FALSE(session);
  EXPECT_FALSE(service_.current_session());
}

TEST_F(TtcDebugToolsServiceTest, SessionsGetTheAudioCap) {
  EXPECT_EQ(service_.BeginSession(nullptr)->recorder().cap_bytes(),
            TtcDebugToolsService::kAudioCapBytes);
}

class TtcDebugToolsServiceFactoryTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(TtcDebugToolsServiceFactoryTest, OnlyCreatedWithBothFeatures) {
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures({kTtc, kTtcDebugTools}, {});
    TestingProfile profile;
    EXPECT_TRUE(TtcDebugToolsService::Get(&profile));
    EXPECT_FALSE(TtcDebugToolsService::Get(
        profile.GetPrimaryOTRProfile(/*create_if_needed=*/true)));
  }
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures({kTtc}, {kTtcDebugTools});
    TestingProfile profile;
    EXPECT_FALSE(TtcDebugToolsService::Get(&profile));
  }
  {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures({kTtcDebugTools}, {kTtc});
    TestingProfile profile;
    EXPECT_FALSE(TtcDebugToolsService::Get(&profile));
  }
}

}  // namespace
}  // namespace ttc
