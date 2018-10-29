// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/chrome_translate_client.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/user_events/fake_user_event_service.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/translate_event.pb.h"
#include "url/gurl.h"

std::unique_ptr<KeyedService> BuildFakeUserEventService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::FakeUserEventService>();
}

metrics::TranslateEventProto BuildTranslateEventProto(
    const std::string& from,
    const std::string& to,
    const metrics::TranslateEventProto::EventType type) {
  metrics::TranslateEventProto event;
  event.set_source_language(from);
  event.set_target_language(to);
  event.set_event_type(type);
  return event;
}

class ChromeTranslateClientTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    fake_user_event_service_ = static_cast<syncer::FakeUserEventService*>(
        browser_sync::UserEventServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                browser_context(),
                base::BindRepeating(&BuildFakeUserEventService)));
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatures(
        {switches::kSyncUserLanguageDetectionEvents,
         switches::kSyncUserTranslationEvents},
        {});
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

 protected:
  syncer::FakeUserEventService* GetUserEventService() {
    return fake_user_event_service_;
  }

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  syncer::FakeUserEventService* fake_user_event_service_;
};

TEST_F(ChromeTranslateClientTest, LanguageEventShouldRecord) {
  const GURL url("http://yahoo.com");
  NavigateAndCommit(url);
  ChromeTranslateClient client(web_contents());
  translate::LanguageDetectionDetails details;
  details.cld_language = "en";
  details.is_cld_reliable = true;
  details.adopted_language = "en";
  client.OnLanguageDetermined(details);
  EXPECT_EQ(1ul, GetUserEventService()->GetRecordedUserEvents().size());
}

TEST_F(ChromeTranslateClientTest, LanguageEventShouldNotRecord) {
  const GURL url("about://blank");
  NavigateAndCommit(url);
  ChromeTranslateClient client(web_contents());
  translate::LanguageDetectionDetails details;
  details.cld_language = "en";
  details.is_cld_reliable = true;
  details.adopted_language = "en";
  client.OnLanguageDetermined(details);
  EXPECT_EQ(0u, GetUserEventService()->GetRecordedUserEvents().size());
}

TEST_F(ChromeTranslateClientTest, TranslationEventShouldRecord) {
  const GURL url("http://yahoo.com");
  NavigateAndCommit(url);
  ChromeTranslateClient client(web_contents());
  // An event we care about.
  const metrics::TranslateEventProto& event_proto = BuildTranslateEventProto(
      "ja", "en", metrics::TranslateEventProto::USER_ACCEPT);
  client.RecordTranslateEvent(event_proto);
  EXPECT_EQ(1ul, GetUserEventService()->GetRecordedUserEvents().size());

  sync_pb::UserEventSpecifics::Translation expected_translation_event;
  expected_translation_event.set_from_language_code("ja");
  expected_translation_event.set_to_language_code("en");
  expected_translation_event.set_interaction(
      sync_pb::UserEventSpecifics::Translation::ACCEPT);
  const auto& result_translation_event =
      GetUserEventService()->GetRecordedUserEvents()[0].translation_event();
  EXPECT_EQ(expected_translation_event.SerializeAsString(),
            result_translation_event.SerializeAsString());
}

TEST_F(ChromeTranslateClientTest, TranslationEventShouldNotRecord) {
  const GURL url("http://yahoo.com");
  NavigateAndCommit(url);
  ChromeTranslateClient client(web_contents());
  // An event we don't care about.
  const metrics::TranslateEventProto& event_proto = BuildTranslateEventProto(
      "ja", "en", metrics::TranslateEventProto::UNSUPPORTED_URL);
  client.RecordTranslateEvent(event_proto);
  EXPECT_EQ(0u, GetUserEventService()->GetRecordedUserEvents().size());
}
