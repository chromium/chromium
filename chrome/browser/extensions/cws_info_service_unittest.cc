// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/cws_info_service.h"

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/cws_info_service_factory.h"
#include "chrome/browser/extensions/cws_item_service.pb.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_urls.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

class CWSInfoServiceTest : public ::testing::Test,
                           public CWSInfoService::Observer {
 protected:
  CWSInfoServiceTest();
  ~CWSInfoServiceTest() override;

  scoped_refptr<const Extension> AddExtension(const std::string& name,
                                              bool updates_from_cws);

  void SetUpResponseWithNetworkError(const GURL& load_url) {
    test_url_loader_factory_.AddResponse(
        load_url, network::mojom::URLResponseHead::New(), std::string(),
        network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));
  }
  void SetUpResponseWithData(const GURL& load_url,
                             const std::string& response) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [](const network::ResourceRequest& request) {}));
    test_url_loader_factory_.AddResponse(load_url.spec(), response);
  }

  StoreMetadata BuildStoreMetadata(const ExtensionId& extension_id,
                                   base::Time last_update_time);
  void VerifyCWSInfoRetrieved(
      const StoreMetadata* metadata,
      const std::optional<CWSInfoService::CWSInfo>& cws_info);

  bool VerifyStats(uint32_t requests,
                   uint32_t responses,
                   uint32_t changes,
                   uint32_t errors) {
    return requests == cws_info_service_->info_requests_ &&
           responses == cws_info_service_->info_responses_ &&
           changes == cws_info_service_->info_changes_ &&
           errors == cws_info_service_->info_errors_;
  }

  int GetTimerCurrentDelay() {
    return cws_info_service_->info_check_timer_.GetCurrentDelay().InSeconds();
  }

  static std::string GetNameFromId(const ExtensionId& id) {
    return "items/" + id + "/storeMetadata";
  }

  static std::unique_ptr<KeyedService> BuildTestContextCWSService(
      content::BrowserContext* context) {
    return std::make_unique<CWSInfoService>(static_cast<Profile*>(context));
  }

  // CWSInfoService::Observer:
  void OnCWSInfoChanged() override {
    info_change_notification_received_ = true;
  }

  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<CWSInfoService> cws_info_service_ = nullptr;
  raw_ptr<ExtensionPrefs> extension_prefs_ = nullptr;
  raw_ptr<ExtensionRegistry> extension_registry_ = nullptr;
  raw_ptr<ExtensionService> extension_service_ = nullptr;
  bool info_change_notification_received_ = false;
};

CWSInfoServiceTest::CWSInfoServiceTest()
    : task_environment_{base::test::TaskEnvironment::TimeSource::MOCK_TIME} {
  auto pref_service =
      std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
  RegisterUserProfilePrefs(pref_service->registry());
  // TODO(anunoy): The following policy pref setting is currently required to
  // enable CWS metadata fetches (see CWSInfoService::CanFetchInfo). Remove this
  // pref setting after `kSafetyCheckExtensions` is enabled by default.
  pref_service->SetInteger(pref_names::kExtensionUnpublishedAvailability, 1);

  TestingProfile::Builder builder;
  builder.SetPrefService(std::move(pref_service));
  builder.SetSharedURLLoaderFactory(
      test_url_loader_factory_.GetSafeWeakWrapper());
  builder.AddTestingFactory(
      CWSInfoServiceFactory::GetInstance(),
      base::BindRepeating(&CWSInfoServiceTest::BuildTestContextCWSService));
  profile_ = builder.Build();
  extension_prefs_ = ExtensionPrefs::Get(profile_.get());
  extension_registry_ = ExtensionRegistry::Get(profile_.get());

  // Create CWSInfoService instance.
  cws_info_service_ = CWSInfoService::Get(profile_.get());

  // Skip official Google API key check for testing.
  cws_info_service_->SetSkipApiCheckForTesting(true);

  // Create test extension service instance.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  auto* test_extension_system = static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(profile_.get()));
  extension_service_ = test_extension_system->CreateExtensionService(
      &command_line, /*install_directory=*/base::FilePath(),
      /*autoupdate_enabled=*/false);
}

CWSInfoServiceTest::~CWSInfoServiceTest() = default;

scoped_refptr<const Extension> CWSInfoServiceTest::AddExtension(
    const std::string& name,
    bool updates_from_cws) {
  ExtensionBuilder builder(name);
  if (updates_from_cws) {
    builder.SetManifestKey("update_url",
                           extension_urls::kChromeWebstoreUpdateURL);
  }
  scoped_refptr<const Extension> extension = builder.Build();
  extension_service_->AddExtension(extension.get());
  return extension;
}

StoreMetadata CWSInfoServiceTest::BuildStoreMetadata(
    const ExtensionId& extension_id,
    base::Time last_update_time) {
  StoreMetadata metadata;
  metadata.set_name(GetNameFromId(extension_id));
  metadata.set_is_live(true);
  metadata.set_last_update_time_millis(
      last_update_time.InMillisecondsSinceUnixEpoch());
  metadata.set_violation_type("none");
  return metadata;
}

void CWSInfoServiceTest::VerifyCWSInfoRetrieved(
    const StoreMetadata* metadata,
    const std::optional<CWSInfoService::CWSInfo>& cws_info) {
  ASSERT_TRUE(cws_info.has_value());
  if (metadata == nullptr) {
    EXPECT_FALSE(cws_info->is_present);
  } else {
    EXPECT_TRUE(cws_info->is_present);
    EXPECT_EQ(metadata->is_live(), cws_info->is_live);
    EXPECT_EQ(base::Time::FromMillisecondsSinceUnixEpoch(
                  metadata->last_update_time_millis()),
              cws_info->last_update_time);
    EXPECT_EQ(
        CWSInfoService::GetViolationTypeFromString(metadata->violation_type()),
        cws_info->violation_type);
    bool no_privacy_practice = false;
    bool unpublished_long_ago = false;
    for (const auto& label : metadata->labels()) {
      if (label == "no-privacy-practice") {
        no_privacy_practice = true;
      } else if (label == "unpublished-long-ago") {
        unpublished_long_ago = true;
      }
    }
    EXPECT_EQ(no_privacy_practice, cws_info->no_privacy_practice);
    EXPECT_EQ(unpublished_long_ago, cws_info->unpublished_long_ago);
  }
}

TEST_F(CWSInfoServiceTest, QueriesCWSExtensions) {
  scoped_refptr<const Extension> test1 =
      AddExtension("test1", /* updates_from_cws= */ true);
  cws_info_service_->CheckAndMaybeFetchInfo();
  ASSERT_EQ(1u, test_url_loader_factory_.pending_requests()->size());
  std::string request_body(test_url_loader_factory_.pending_requests()
                               ->at(0)
                               .request.request_body->elements()
                               ->at(0)
                               .As<network::DataElementBytes>()
                               .AsStringPiece());
  EXPECT_TRUE(VerifyStats(/*requests=*/1, 0, 0, 0));
  BatchGetStoreMetadatasRequest request_proto;
  ASSERT_TRUE(request_proto.ParseFromString(request_body));
  ASSERT_EQ(1, request_proto.names_size());
  EXPECT_EQ(GetNameFromId(test1->id()), request_proto.names(0));
}

TEST_F(CWSInfoServiceTest, IgnoresNonCWSExtensions) {
  AddExtension("test1", /* updates_from_cws= */ false);
  cws_info_service_->CheckAndMaybeFetchInfo();
  EXPECT_TRUE(VerifyStats(/*requests=*/0, 0, 0, 0));
  EXPECT_EQ(0u, test_url_loader_factory_.pending_requests()->size());
}

TEST_F(CWSInfoServiceTest, HandlesNetworkErrorAndBadServerResponse) {
  base::HistogramTester histogram_tester;
  scoped_refptr<const Extension> test1 =
      AddExtension("test1", /* updates_from_cws= */ true);
  SetUpResponseWithNetworkError(
      GURL(cws_info_service_->GetRequestURLForTesting()));
  cws_info_service_->CheckAndMaybeFetchInfo();
  task_environment_.FastForwardBy(base::Seconds(0));

  // Verify an errored response was received.
  EXPECT_TRUE(VerifyStats(/*requests=*/1, /*responses=*/0, /*changes=*/0,
                          /*errors=*/1));
  histogram_tester.ExpectBucketCount(
      "Extensions.CWSInfoService.NetworkResponseCodeOrError",
      net::HTTP_NOT_FOUND, 1);
  histogram_tester.ExpectBucketCount("Extensions.CWSInfoService.FetchSuccess",
                                     false, 1);
  EXPECT_TRUE(cws_info_service_->GetCWSInfo(*test1) == std::nullopt);
  // Verify that the fetch error timestamp was recorded.
  EXPECT_EQ(base::Time::Now(),
            cws_info_service_->GetCWSInfoFetchErrorTimestampForTesting());

  // After a response error, the next fetch request is only made after
  // another fetch interval has elapsed. Advance the time by that amount.
  task_environment_.FastForwardBy(
      base::Seconds(cws_info_service_->GetFetchIntervalForTesting()));
  SetUpResponseWithData(GURL(cws_info_service_->GetRequestURLForTesting()),
                        "bad response");
  cws_info_service_->CheckAndMaybeFetchInfo();
  task_environment_.FastForwardBy(base::Seconds(0));
  EXPECT_TRUE(VerifyStats(/*requests=*/2, /*responses=*/0, /*changes=*/0,
                          /*errors=*/2));
  histogram_tester.ExpectBucketCount(
      "Extensions.CWSInfoService.NetworkResponseCodeOrError", net::HTTP_OK, 1);
  histogram_tester.ExpectBucketCount("Extensions.CWSInfoService.FetchSuccess",
                                     false, 2);
  EXPECT_TRUE(cws_info_service_->GetCWSInfo(*test1) == std::nullopt);
}

TEST_F(CWSInfoServiceTest, SavesGoodResponse) {
  base::HistogramTester histogram_tester;
  scoped_refptr<const Extension> test1 =
      AddExtension("test1", /*updates_from_cws=*/true);
  base::Time last_update_time = base::Time::Now() - base::Days(31);
  BatchGetStoreMetadatasResponse response_proto;
  *response_proto.add_store_metadatas() =
      BuildStoreMetadata(test1->id(), last_update_time);
  std::string response_str = response_proto.SerializeAsString();
  ASSERT_TRUE(!response_str.empty());
  SetUpResponseWithData(GURL(cws_info_service_->GetRequestURLForTesting()),
                        response_str);
  cws_info_service_->AddObserver(this);
  cws_info_service_->CheckAndMaybeFetchInfo();
  task_environment_.FastForwardBy(base::Seconds(0));

  EXPECT_TRUE(VerifyStats(/*requests=*/1, /*responses=*/1, /*changes=*/1,
                          /*errors=*/0));
  EXPECT_EQ(base::Time::Now(),
            cws_info_service_->GetCWSInfoTimestampForTesting());
  EXPECT_TRUE(info_change_notification_received_);
  histogram_tester.ExpectBucketCount(
      "Extensions.CWSInfoService.NetworkResponseCodeOrError", net::HTTP_OK, 1);
  histogram_tester.ExpectBucketCount(
      "Extensions.CWSInfoService.NumRequestsInFetch", /*requests=*/1, 1);
  histogram_tester.ExpectBucketCount(
      "Extensions.CWSInfoService.NetworkRetriesTillSuccess", 0, 1);
  histogram_tester.ExpectBucketCount("Extensions.CWSInfoService.FetchSuccess",
                                     true, 1);
  histogram_tester.ExpectBucketCount(
      "Extensions.CWSInfoService.MetadataChanged", true, 1);

  std::optional<CWSInfoService::CWSInfo> info =
      cws_info_service_->GetCWSInfo(*test1);
  VerifyCWSInfoRetrieved(&response_proto.store_metadatas(0), info);
}

TEST_F(CWSInfoServiceTest, HandlesMultipleRequestsPerInfoCheck) {
  base::HistogramTester histogram_tester;
  // Set max of 2 extension ids per request.
  cws_info_service_->SetMaxExtensionIdsPerRequestForTesting(2);

  // Add 3 extensions.
  scoped_refptr<const Extension> test1 =
      AddExtension("test1", /*updates_from_cws=*/true);
  scoped_refptr<const Extension> test2 =
      AddExtension("test2", /* updates_from_cws= */ true);
  scoped_refptr<const Extension> test3 =
      AddExtension("test3", /*updates_from_cws=*/true);

  // Build store metadata for 1st extension.
  base::Time test1_last_update_time = base::Time::Now() - base::Days(1);
  StoreMetadata test1_metadata =
      BuildStoreMetadata(test1->id(), test1_last_update_time);
  // Override builder defaults.
  test1_metadata.set_is_live(false);
  test1_metadata.set_violation_type("policy-violation");
  test1_metadata.add_labels("no-privacy-practice");

  // Build store metadata for 2nd extension.
  base::Time test2_last_update_time = base::Time::Now() - base::Days(31);
  StoreMetadata test2_metadata =
      BuildStoreMetadata(test2->id(), test2_last_update_time);

  // Override builder defaults.
  test2_metadata.set_is_live(false);
  test2_metadata.set_violation_type("malware");
  test2_metadata.add_labels("unpublished-long-ago");
  test2_metadata.add_labels("no-privacy-practice");
  // Create response proto with metadata for only 2 extensions.
  BatchGetStoreMetadatasResponse response;
  *response.add_store_metadatas() = test1_metadata;
  *response.add_store_metadatas() = test2_metadata;
  std::string response_str = response.SerializeAsString();
  ASSERT_TRUE(!response_str.empty());

  // Set up server response for requests and start the info check.
  SetUpResponseWithData(GURL(cws_info_service_->GetRequestURLForTesting()),
                        response_str);
  cws_info_service_->CheckAndMaybeFetchInfo();
  task_environment_.FastForwardBy(base::Seconds(0));

  // Verify info request, received, changes stats.
  EXPECT_EQ(2u, test_url_loader_factory_.total_requests());
  EXPECT_TRUE(VerifyStats(/*requests=*/2, /*responses=*/2, /*changes=*/2,
                          /*errors=*/0));
  histogram_tester.ExpectBucketCount(
      "Extensions.CWSInfoService.NumRequestsInFetch", /*requests=*/2, 1);
  histogram_tester.ExpectBucketCount("Extensions.CWSInfoService.FetchSuccess",
                                     true, 1);

  // Retrieve information for 1st extension and verify.
  std::optional<CWSInfoService::CWSInfo> info =
      cws_info_service_->GetCWSInfo(*test1);
  VerifyCWSInfoRetrieved(&test1_metadata, info);

  // Retrieve information for 2nd extension and verify.
  info = cws_info_service_->GetCWSInfo(*test2);
  VerifyCWSInfoRetrieved(&test2_metadata, info);

  // Retrieve information for 3rd extension and verify.
  info = cws_info_service_->GetCWSInfo(*test3);
  VerifyCWSInfoRetrieved(nullptr, info);
}

TEST_F(CWSInfoServiceTest, SchedulesStartupAndPeriodicInfoChecks) {
  // Add an extension to cause queries to CWS.
  scoped_refptr<const Extension> test1 =
      AddExtension("test1", /*updates_from_cws=*/true);

  // Verify that the first info check is scheduled with the startup delay.
  EXPECT_EQ(cws_info_service_->GetStartupDelayForTesting(),
            GetTimerCurrentDelay());
  SetUpResponseWithNetworkError(
      GURL(cws_info_service_->GetRequestURLForTesting()));
  task_environment_.FastForwardBy(
      base::Seconds(cws_info_service_->GetStartupDelayForTesting()));
  // Verify that a request was sent and an errored response was received.
  EXPECT_TRUE(VerifyStats(/*requests=*/1, /*responses=*/0, /*changes=*/0,
                          /*errors=*/1));
  // Verify that the subsequent info check is scheduled with the regular check
  // interval.
  EXPECT_EQ(cws_info_service_->GetCheckIntervalForTesting(),
            GetTimerCurrentDelay());

  // Advance the time by check interval and verify that a request is not sent
  // because of the previous fetch response error.
  task_environment_.FastForwardBy(
      base::Seconds(cws_info_service_->GetCheckIntervalForTesting()));
  EXPECT_TRUE(VerifyStats(/*requests=*/1, /*responses=*/0, /*changes=*/0,
                          /*errors=*/1));
  // Check that nothing was written to extension prefs.
  EXPECT_EQ(base::Time(), cws_info_service_->GetCWSInfoTimestampForTesting());

  // Verify that the subsequent info check is scheduled with the regular check
  // interval.
  EXPECT_EQ(cws_info_service_->GetCheckIntervalForTesting(),
            GetTimerCurrentDelay());

  // Set up a valid response from the server.
  base::Time last_update_time = base::Time::Now() - base::Days(31);
  BatchGetStoreMetadatasResponse response_proto;
  *response_proto.add_store_metadatas() =
      BuildStoreMetadata(test1->id(), last_update_time);
  std::string response_str = response_proto.SerializeAsString();
  ASSERT_TRUE(!response_str.empty());
  SetUpResponseWithData(GURL(cws_info_service_->GetRequestURLForTesting()),
                        response_str);
  // Forward time by the fetch interval since CWSInfoService will wait that
  // long after a fetch error before sending another request.
  task_environment_.FastForwardBy(
      base::Seconds(cws_info_service_->GetFetchIntervalForTesting()));
  // Verify that the request was sent, response was received and the data was
  // saved to extension prefs.
  EXPECT_TRUE(VerifyStats(/*requests=*/2, /*responses=*/1, /*changes=*/1,
                          /*errors=*/1));
  EXPECT_NE(base::Time(), cws_info_service_->GetCWSInfoTimestampForTesting());
  // Verify that the next check is scheduled with the regular check interval.
  EXPECT_EQ(cws_info_service_->GetCheckIntervalForTesting(),
            GetTimerCurrentDelay());
}

// If there are no new extensions installed, CWS Info is only
// requested after a fetch interval has elapsed.
TEST_F(CWSInfoServiceTest, UpdatesExistingInfoAtUpdateIntervals) {
  // Add an extension to cause queries to CWS.
  scoped_refptr<const Extension> test1 =
      AddExtension("test1", /*updates_from_cws=*/true);

  // Set up a valid response from the server.
  base::Time last_update_time = base::Time::Now() - base::Days(31);
  BatchGetStoreMetadatasResponse response_proto;
  *response_proto.add_store_metadatas() =
      BuildStoreMetadata(test1->id(), last_update_time);
  std::string response_str = response_proto.SerializeAsString();
  ASSERT_TRUE(!response_str.empty());
  SetUpResponseWithData(GURL(cws_info_service_->GetRequestURLForTesting()),
                        response_str);
  task_environment_.FastForwardBy(
      base::Seconds(cws_info_service_->GetStartupDelayForTesting()));

  // Verify that the request was sent, response was received and the data was
  // saved to extension prefs.
  EXPECT_TRUE(VerifyStats(/*requests=*/1, /*responses=*/1, /*changes=*/1,
                          /*errors=*/0));
  EXPECT_EQ(base::Time::Now(),
            cws_info_service_->GetCWSInfoTimestampForTesting());

  // Verify that no request is sent at the next check interval since the
  // fetch interval has not elapsed.
  task_environment_.FastForwardBy(
      base::Seconds(cws_info_service_->GetCheckIntervalForTesting()));
  EXPECT_TRUE(VerifyStats(/*requests=*/1, /*responses=*/1, /*changes=*/1,
                          /*errors=*/0));
  EXPECT_EQ(base::Time::Now() -
                base::Seconds(cws_info_service_->GetCheckIntervalForTesting()),
            cws_info_service_->GetCWSInfoTimestampForTesting());

  // Verify that a request is sent once the fetch interval has elapsed.
  // Already consumed 1 check interval; compute the rest till the next fetch.
  int remaining_check_intervals_till_next_fetch =
      cws_info_service_->GetFetchIntervalForTesting() /
      cws_info_service_->GetCheckIntervalForTesting();
  task_environment_.FastForwardBy(
      base::Seconds(cws_info_service_->GetCheckIntervalForTesting() *
                    remaining_check_intervals_till_next_fetch));
  // Note the info changed count has not changed since the server response is
  // the same.
  EXPECT_TRUE(VerifyStats(/*requests=*/2, /*responses=*/2, /*changes=*/1,
                          /*errors=*/0));
  EXPECT_EQ(base::Time::Now(),
            cws_info_service_->GetCWSInfoTimestampForTesting());
}

}  // namespace extensions
