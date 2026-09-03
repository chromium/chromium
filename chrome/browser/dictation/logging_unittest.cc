// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/logging.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/dictation_keyed_service_factory.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/common/extensions/api/dictation_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/test_event_router_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dictation {

namespace {

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* context) {
  return std::make_unique<extensions::EventRouter>(context, nullptr);
}

std::unique_ptr<KeyedService> BuildDictationKeyedService(
    content::BrowserContext* context) {
  return std::make_unique<DictationKeyedService>(
      Profile::FromBrowserContext(context));
}

}  // namespace

class DictationLoggingTest : public testing::Test {
 public:
  DictationLoggingTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        scoped_feature_list_(CreateEnablingFeatureList()) {
    extensions::EventRouterFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildEventRouter));
    DictationKeyedServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildDictationKeyedService));
    profile_.GetPrefs()->SetBoolean(prefs::kPrefDictationOnboardingCompleted,
                                    true);
  }
  ~DictationLoggingTest() override = default;

  DictationKeyedService* service() {
    return DictationKeyedServiceFactory::GetDictationKeyedService(&profile_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfile profile_;
};

TEST_F(DictationLoggingTest, BuffersAndFlushesOnTimer) {
  extensions::TestEventRouterObserver observer(
      extensions::EventRouter::Get(&profile_));

  VT_LOG(&profile_) << "Log message 1";
  VT_LOG(&profile_) << "Log message 2";

  // Before the timer expires, no events should be dispatched.
  EXPECT_TRUE(observer.events().empty());

  // Fast-forward past the flush interval.
  task_environment_.FastForwardBy(DictationLogBuffer::kFlushInterval);

  // Exactly one batched event should have been dispatched.
  EXPECT_EQ(observer.all_events().size(), 1u);
  auto it = observer.events().find(
      extensions::api::dictation_private::OnBrowserLog::kEventName);
  ASSERT_NE(it, observer.events().end());
  ASSERT_TRUE(it->second);
  ASSERT_EQ(it->second->args().size(), 1u);

  const base::ListValue& list = it->second->args()[0].GetList();
  ASSERT_EQ(list.size(), 2u);
  const std::string* msg1 = list[0].GetDict().FindString("message");
  const std::string* msg2 = list[1].GetDict().FindString("message");
  ASSERT_NE(msg1, nullptr);
  ASSERT_NE(msg2, nullptr);
  EXPECT_EQ(*msg1, "Log message 1");
  EXPECT_EQ(*msg2, "Log message 2");

  // Fast-forward again to ensure no additional events are dispatched.
  task_environment_.FastForwardBy(DictationLogBuffer::kFlushInterval);
  EXPECT_EQ(observer.all_events().size(), 1u);
}

TEST_F(DictationLoggingTest, ExplicitFlushSendsImmediately) {
  extensions::TestEventRouterObserver observer(
      extensions::EventRouter::Get(&profile_));

  VT_LOG(&profile_) << "Immediate log message";
  EXPECT_TRUE(observer.events().empty());

  service()->log_buffer().Flush();

  EXPECT_EQ(observer.all_events().size(), 1u);
  auto it = observer.events().find(
      extensions::api::dictation_private::OnBrowserLog::kEventName);
  ASSERT_NE(it, observer.events().end());
}

}  // namespace dictation
