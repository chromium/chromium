// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_metrics.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/private_insights/private_insights_service_factory.h"
#include "chrome/browser/storage_access_api/storage_access_metrics_unittest_psl-reversed-inc.cc"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/private_insights/events/storage_access_log_event.pb.h"
#include "components/private_insights/private_insights_features.h"
#include "components/private_insights/private_insights_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace storage_access_api {

namespace {

using ::private_insights::PrivateInsightsService;
using ::private_insights::PrivateInsightsServiceFactory;
using ::private_insights::events::StorageAccessLogEvent;
using ::testing::_;
using ::testing::Optional;
using ::testing::Property;

net::SchemefulSite Site(std::string_view url) {
  return net::SchemefulSite(GURL(url));
}

class MockPrivateInsightsService : public PrivateInsightsService {
 public:
  MockPrivateInsightsService(PrefService* local_state,
                             const base::FilePath& profile_dir)
      : PrivateInsightsService(local_state,
                               profile_dir,
                               /*url_loader_factory=*/nullptr) {}

  MOCK_METHOD(void,
              LogStorageAccessEvent,
              (StorageAccessLogEvent event),
              (override));
};

class StorageAccessMetricsTest : public testing::Test {
 protected:
  explicit StorageAccessMetricsTest(bool storage_access_enabled = true) {
    // Frozen Public Suffix List; see storage_access_metrics_unittest_psl.gperf.
    net::registry_controlled_domains::SetFindDomainGraphForTesting(
        storage_access_api_test_psl::kDafsa);
    feature_list_.InitWithFeatureState(
        private_insights::kPrivateInsightsStorageAccess,
        storage_access_enabled);

    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        PrivateInsightsServiceFactory::GetInstance(),
        base::BindOnce(&StorageAccessMetricsTest::BuildService,
                       base::Unretained(this)));
    profile_ = builder.Build();
    // Built eagerly so tests can set expectations before recording.
    PrivateInsightsServiceFactory::GetForProfile(profile_.get());
    CHECK(service_);
  }

  ~StorageAccessMetricsTest() override {
    service_ = nullptr;
    profile_.reset();
    net::registry_controlled_domains::ResetFindDomainGraphForTesting();
  }

  std::unique_ptr<KeyedService> BuildService(content::BrowserContext* context) {
    auto service =
        std::make_unique<testing::StrictMock<MockPrivateInsightsService>>(
            &local_state_, context->GetPath());
    service_ = service.get();
    return service;
  }

  // Records `outcome` for `url` and returns the logged event, if any.
  std::optional<StorageAccessLogEvent> Record(
      std::string_view url,
      RequestOutcome outcome = RequestOutcome::kGrantedByUser) {
    std::optional<StorageAccessLogEvent> logged;
    EXPECT_CALL(*service_, LogStorageAccessEvent(_))
        .Times(testing::AtMost(1))
        .WillOnce([&](StorageAccessLogEvent event) { logged = event; });
    RecordOutcomeToPrivateInsights(profile_.get(), outcome, Site(url));
    testing::Mock::VerifyAndClearExpectations(service_);
    return logged;
  }

  std::optional<std::string> RecordedDomain(std::string_view url) {
    std::optional<StorageAccessLogEvent> event = Record(url);
    if (!event) {
      return std::nullopt;
    }
    return event->domain();
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<testing::StrictMock<MockPrivateInsightsService>> service_ = nullptr;
};

TEST_F(StorageAccessMetricsTest, LogsEvent) {
  std::optional<StorageAccessLogEvent> event =
      Record("https://www.embed-widget.com", RequestOutcome::kGrantedByUser);
  ASSERT_TRUE(event.has_value());
  EXPECT_EQ(event->domain(), "embed-widget.com");
  EXPECT_EQ(event->outcome(), StorageAccessLogEvent::GRANTED_BY_USER);
  EXPECT_EQ(event->system_profile().chrome_channel(),
            version_info::GetChannelString(chrome::GetChannel()));
  // Only the channel is reported.
  EXPECT_EQ(event->system_profile().country(), "");
  EXPECT_EQ(event->system_profile().language(), "");
  EXPECT_EQ(event->system_profile().chrome_version(), "");
  EXPECT_EQ(event->system_profile().milestone(), "");
  EXPECT_EQ(event->system_profile().platform(), "");
}

TEST_F(StorageAccessMetricsTest, DomainIsETldPlusOne) {
  EXPECT_EQ(RecordedDomain("https://embed-widget.com"), "embed-widget.com");
  EXPECT_EQ(RecordedDomain("https://a.b.embed-widget.com"), "embed-widget.com");
  EXPECT_EQ(RecordedDomain("https://sub.example.co.uk"), "example.co.uk");
  EXPECT_EQ(RecordedDomain("http://embed-widget.com"), "embed-widget.com");
}

TEST_F(StorageAccessMetricsTest, PrivateRegistriesAreIgnored) {
  // Only ICANN registries are used; github.io is a private one.
  EXPECT_EQ(RecordedDomain("https://alice.github.io"), "github.io");
  EXPECT_EQ(RecordedDomain("https://github.io"), "github.io");
}

TEST_F(StorageAccessMetricsTest, UnknownRegistriesAreDropped) {
  EXPECT_EQ(RecordedDomain("https://sso.acme.corp"), std::nullopt);
  EXPECT_EQ(RecordedDomain("http://intranet"), std::nullopt);
  EXPECT_EQ(RecordedDomain("http://localhost"), std::nullopt);
}

TEST_F(StorageAccessMetricsTest, PublicSuffixesAndIpsAreDropped) {
  EXPECT_EQ(RecordedDomain("https://co.uk"), std::nullopt);
  EXPECT_EQ(RecordedDomain("https://com"), std::nullopt);
  EXPECT_EQ(RecordedDomain("http://10.0.0.1"), std::nullopt);
  EXPECT_EQ(RecordedDomain("http://[::1]"), std::nullopt);
}

TEST_F(StorageAccessMetricsTest, OpaqueAndNonHttpSitesAreDropped) {
  EXPECT_CALL(*service_, LogStorageAccessEvent).Times(0);
  RecordOutcomeToPrivateInsights(profile_.get(), RequestOutcome::kGrantedByUser,
                                 net::SchemefulSite());
  testing::Mock::VerifyAndClearExpectations(service_);
  EXPECT_EQ(RecordedDomain("file:///tmp/a.html"), std::nullopt);
  EXPECT_EQ(RecordedDomain("data:text/html,hi"), std::nullopt);
  // Has a host with a public registry, but is not HTTP(S).
  EXPECT_EQ(RecordedDomain("ftp://example.com"), std::nullopt);
}

TEST_F(StorageAccessMetricsTest, OutcomeMapping) {
  const std::vector<std::pair<RequestOutcome, StorageAccessLogEvent::Outcome>>
      kCases = {
          {RequestOutcome::kGrantedByAllowance,
           StorageAccessLogEvent::GRANTED_BY_ALLOWANCE},
          {RequestOutcome::kGrantedByUser,
           StorageAccessLogEvent::GRANTED_BY_USER},
          {RequestOutcome::kDeniedByUser,
           StorageAccessLogEvent::DENIED_BY_USER},
          {RequestOutcome::kDeniedByPrerequisites,
           StorageAccessLogEvent::DENIED_BY_PREREQUISITES},
          {RequestOutcome::kDismissedByUser,
           StorageAccessLogEvent::DISMISSED_BY_USER},
          {RequestOutcome::kReusedPreviousDecision,
           StorageAccessLogEvent::REUSED_PREVIOUS_DECISION},
          {RequestOutcome::kDeniedByTopLevelInteractionHeuristic,
           StorageAccessLogEvent::DENIED_BY_TOP_LEVEL_INTERACTION_HEURISTIC},
          {RequestOutcome::kAllowedByCookieSettings,
           StorageAccessLogEvent::ALLOWED_BY_COOKIE_SETTINGS},
          {RequestOutcome::kReusedImplicitGrant,
           StorageAccessLogEvent::REUSED_IMPLICIT_GRANT},
          {RequestOutcome::kDeniedByCookieSettings,
           StorageAccessLogEvent::DENIED_BY_COOKIE_SETTINGS},
          {RequestOutcome::kAllowedBySameSite,
           StorageAccessLogEvent::ALLOWED_BY_SAME_SITE},
          {RequestOutcome::kDeniedAborted,
           StorageAccessLogEvent::DENIED_ABORTED},
          {RequestOutcome::kAllowedByFedCM,
           StorageAccessLogEvent::ALLOWED_BY_FEDCM},
      };
  for (const auto& [outcome, expected] : kCases) {
    SCOPED_TRACE("RequestOutcome " +
                 base::NumberToString(static_cast<int>(outcome)));
    EXPECT_THAT(Record("https://embed-widget.com", outcome),
                Optional(Property(&StorageAccessLogEvent::outcome, expected)));
  }
}

// Every RequestOutcome maps to a distinct, non-UNSPECIFIED value.
TEST_F(StorageAccessMetricsTest, EveryOutcomeIsDistinctAndSpecified) {
  std::set<StorageAccessLogEvent::Outcome> seen;
  for (int i = 0; i <= static_cast<int>(RequestOutcome::kMaxValue); ++i) {
    if (i == 0 || i == 3) {
      continue;  // k{Granted,Denied}ByFirstPartySet, removed.
    }
    SCOPED_TRACE("index " + base::NumberToString(i));
    std::optional<StorageAccessLogEvent> event =
        Record("https://embed-widget.com", static_cast<RequestOutcome>(i));
    ASSERT_TRUE(event.has_value());
    EXPECT_NE(event->outcome(), StorageAccessLogEvent::UNSPECIFIED);
    EXPECT_TRUE(StorageAccessLogEvent::Outcome_IsValid(event->outcome()));
    EXPECT_TRUE(seen.insert(event->outcome()).second);
  }
}

// Off-the-record profiles have no PrivateInsightsService.
TEST_F(StorageAccessMetricsTest, NotLoggedOffTheRecord) {
  Profile* otr_profile =
      profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_EQ(PrivateInsightsServiceFactory::GetForProfile(otr_profile), nullptr);
  EXPECT_CALL(*service_, LogStorageAccessEvent).Times(0);
  RecordOutcomeToPrivateInsights(otr_profile, RequestOutcome::kGrantedByUser,
                                 Site("https://embed-widget.com"));
}

class StorageAccessMetricsFeatureDisabledTest
    : public StorageAccessMetricsTest {
 protected:
  StorageAccessMetricsFeatureDisabledTest()
      : StorageAccessMetricsTest(/*storage_access_enabled=*/false) {}
};

TEST_F(StorageAccessMetricsFeatureDisabledTest, NotLogged) {
  EXPECT_CALL(*service_, LogStorageAccessEvent).Times(0);
  RecordOutcomeToPrivateInsights(profile_.get(), RequestOutcome::kGrantedByUser,
                                 Site("https://embed-widget.com"));
}

}  // namespace

}  // namespace storage_access_api
