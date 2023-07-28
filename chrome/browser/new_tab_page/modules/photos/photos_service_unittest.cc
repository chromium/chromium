// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/photos/photos_service.h"
#include "base/barrier_closure.h"
#include "base/hash/hash.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "ui/base/l10n/l10n_util.h"

class PhotosServiceTest : public testing::Test {
 public:
  PhotosServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    testing::Test::SetUp();
    service_ = std::make_unique<PhotosService>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env.identity_manager(), &prefs_);
    identity_test_env.MakePrimaryAccountAvailable(
        "example@google.com", signin::ConsentLevel::kSignin);
    service_->RegisterProfilePrefs(prefs_.registry());
  }

  void TearDown() override {
    service_.reset();
    test_url_loader_factory_.ClearResponses();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<PhotosService> service_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  signin::IdentityTestEnvironment identity_test_env;
  TestingPrefServiceSimple prefs_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PhotosServiceTest, PassesDataOnSuccess) {
  auto quit_closure = task_environment_.QuitClosure();
  std::vector<photos::mojom::MemoryPtr> actual_memories;
  base::MockCallback<PhotosService::GetMemoriesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<photos::mojom::MemoryPtr> memories) {
            actual_memories = std::move(memories);
            quit_closure.Run();
          }));

  // Make sure we are not in the dismissed time window by default.
  prefs_.SetTime(PhotosService::kLastDismissedTimePrefName, base::Time::Now());
  task_environment_.AdvanceClock(PhotosService::kDismissDuration);
  service_->GetMemories(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());
  EXPECT_EQ(1, test_url_loader_factory_.NumPending());
  EXPECT_EQ(
      test_url_loader_factory_.pending_requests()
          ->at(0)
          .request.headers.ToString(),
      "Content-Type: application/json\r\nAuthorization: Bearer foo\r\n\r\n");

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://photosfirstparty-pa.googleapis.com/v1/ntp/memories:read",
      R"(
        {
          "memory": [
            {
              "memoryMediaKey": "key1",
              "title": {
                "header": "Title 1",
                "subheader": "Something something 1"
              },
              "coverMediaKey": "coverKey1",
              "coverUrl": "https://photos.google.com/img/coverKey1"
            },
            {
              "memoryMediaKey": "key2",
              "title": {
                "header": "Title 2",
                "subheader": "Something something 2"
              },
              "coverMediaKey": "coverKey2",
              "coverUrl": "https://photos.google.com/img/coverKey2"
            }
          ]
        }
      )",
      net::HTTP_OK,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);
  task_environment_.RunUntilQuit();

  EXPECT_EQ(2u, actual_memories.size());
  EXPECT_EQ("Title 1", actual_memories.at(0)->title);
  EXPECT_EQ("key1", actual_memories.at(0)->id);
  EXPECT_EQ(
      "https://photos.google.com/memory/featured/key1/photo/"
      "coverKey1?referrer=CHROME_NTP",
      actual_memories.at(0)->item_url);
  EXPECT_EQ("https://photos.google.com/img/coverKey1?access_token=foo",
            actual_memories.at(0)->cover_url);
  EXPECT_EQ("Title 2", actual_memories.at(1)->title);
  EXPECT_EQ("key2", actual_memories.at(1)->id);
  EXPECT_EQ(
      "https://photos.google.com/memory/featured/key2/photo/"
      "coverKey2?referrer=CHROME_NTP",
      actual_memories.at(1)->item_url);
  EXPECT_EQ("https://photos.google.com/img/coverKey2?access_token=foo",
            actual_memories.at(1)->cover_url);
  ASSERT_EQ(1,
            histogram_tester_.GetBucketCount("NewTabPage.Modules.DataRequest",
                                             base::PersistentHash("photos")));
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Photos.DataRequest",
                   PhotosService::RequestResult::kSuccess));
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Photos.DataResponseCount", 2));
}

TEST_F(PhotosServiceTest, RequestIsCached) {
  auto quit_closure = task_environment_.QuitClosure();
  std::vector<photos::mojom::MemoryPtr> actual_memories;
  base::MockCallback<PhotosService::GetMemoriesCallback> callback;

  network::URLLoaderCompletionStatus status;
  status.exists_in_cache = true;
  test_url_loader_factory_.AddResponse(
      GURL("https://photosfirstparty-pa.googleapis.com/v1/ntp/memories:read"),
      network::CreateURLResponseHead(net::HTTP_OK),
      R"(
        {
          "memory": [
            {
              "memoryMediaKey": "key1",
              "title": {
                "header": "Title 1",
                "subheader": "Something something 1"
              },
              "coverMediaKey": "coverKey1",
              "coverUrl": "https://photos.google.com/img/coverKey1"
            },
            {
              "memoryMediaKey": "key2",
              "title": {
                "header": "Title 2",
                "subheader": "Something something 2"
              },
              "coverMediaKey": "coverKey2",
              "coverUrl": "https://photos.google.com/img/coverKey2"
            }
          ]
        }
      )",
      status);

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<photos::mojom::MemoryPtr> memories) {
            actual_memories = std::move(memories);
            quit_closure.Run();
          }));
  service_->GetMemories(callback.Get());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());
  task_environment_.RunUntilQuit();

  EXPECT_EQ(2u, actual_memories.size());
  ASSERT_EQ(0,
            histogram_tester_.GetBucketCount("NewTabPage.Modules.DataRequest",
                                             base::PersistentHash("photos")));
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Photos.DataRequest",
                   PhotosService::RequestResult::kCached));
}

TEST_F(PhotosServiceTest, CacheIsSkippedOnMemoryOpen) {
  auto quit_closure = task_environment_.QuitClosure();
  std::vector<photos::mojom::MemoryPtr> actual_memories;
  base::MockCallback<PhotosService::GetMemoriesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(2)
      .WillRepeatedly(
          testing::Invoke([&](std::vector<photos::mojom::MemoryPtr> memories) {
            actual_memories = std::move(memories);
            quit_closure.Run();
          }));

  network::URLLoaderCompletionStatus status;
  status.exists_in_cache = true;
  test_url_loader_factory_.AddResponse(
      GURL("https://photosfirstparty-pa.googleapis.com/v1/ntp/memories:read"),
      network::CreateURLResponseHead(net::HTTP_OK),
      R"(
        {
          "memory": [
            {
              "memoryMediaKey": "key1",
              "title": {
                "header": "Title 1",
                "subheader": "Something something 1"
              },
              "coverMediaKey": "coverKey1",
              "coverUrl": "https://photos.google.com/img/coverKey1"
            },
            {
              "memoryMediaKey": "key2",
              "title": {
                "header": "Title 2",
                "subheader": "Something something 2"
              },
              "coverMediaKey": "coverKey2",
              "coverUrl": "https://photos.google.com/img/coverKey2"
            }
          ]
        }
      )",
      status);

  service_->GetMemories(callback.Get());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());
  task_environment_.RunUntilQuit();

  // API response is cached
  EXPECT_EQ(2u, actual_memories.size());
  ASSERT_EQ(0,
            histogram_tester_.GetBucketCount("NewTabPage.Modules.DataRequest",
                                             base::PersistentHash("photos")));
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Photos.DataRequest",
                   PhotosService::RequestResult::kCached));

  // Save last time memory was opened as a timestamp
  EXPECT_TRUE(
      prefs_.GetTime(PhotosService::kLastMemoryOpenTimePrefName).is_null());
  service_->OnMemoryOpen();
  EXPECT_FALSE(
      prefs_.GetTime(PhotosService::kLastMemoryOpenTimePrefName).is_null());

  // Expecting new API call with last opened timestamp in URL
  quit_closure = task_environment_.QuitClosure();
  base::Time now = base::Time::Now();
  prefs_.SetTime(PhotosService::kLastMemoryOpenTimePrefName, now);
  service_->GetMemories(callback.Get());
  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());
  std::string url =
      "https://photosfirstparty-pa.googleapis.com/v1/ntp/"
      "memories:read?lastViewed=" +
      base::NumberToString(now.ToTimeT());
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      url,
      R"(
        {
          "memory": [
            {
              "memoryMediaKey": "key1",
              "title": {
                "header": "Title 1",
                "subheader": "Something something 1"
              },
              "coverMediaKey": "coverKey1",
              "coverUrl": "https://photos.google.com/img/coverKey1"
            }
          ]
        }
      )",
      net::HTTP_OK,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);
  task_environment_.RunUntilQuit();

  EXPECT_EQ(1u, actual_memories.size());
  ASSERT_EQ(1,
            histogram_tester_.GetBucketCount("NewTabPage.Modules.DataRequest",
                                             base::PersistentHash("photos")));
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Photos.DataRequest",
                   PhotosService::RequestResult::kSuccess));
}

TEST_F(PhotosServiceTest, PassesDataToMultipleRequestsToPhotosService) {
  auto quit_closure = task_environment_.QuitClosure();
  auto barrier_closure = base::BarrierClosure(4, quit_closure);

  std::vector<photos::mojom::MemoryPtr> response1;
  std::vector<photos::mojom::MemoryPtr> response2;
  std::vector<photos::mojom::MemoryPtr> response3;
  std::vector<photos::mojom::MemoryPtr> response4;

  base::MockCallback<PhotosService::GetMemoriesCallback> callback1;
  base::MockCallback<PhotosService::GetMemoriesCallback> callback2;
  base::MockCallback<PhotosService::GetMemoriesCallback> callback3;
  base::MockCallback<PhotosService::GetMemoriesCallback> callback4;
  EXPECT_CALL(callback1, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<photos::mojom::MemoryPtr> memories) {
            response1 = std::move(memories);
            barrier_closure.Run();
          }));
  EXPECT_CALL(callback2, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<photos::mojom::MemoryPtr> memories) {
            response2 = std::move(memories);
            barrier_closure.Run();
          }));
  EXPECT_CALL(callback3, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<photos::mojom::MemoryPtr> memories) {
            response3 = std::move(memories);
            barrier_closure.Run();
          }));
  EXPECT_CALL(callback4, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<photos::mojom::MemoryPtr> memories) {
            response4 = std::move(memories);
            barrier_closure.Run();
          }));
  service_->GetMemories(callback1.Get());
  service_->GetMemories(callback2.Get());
  service_->GetMemories(callback3.Get());
  service_->GetMemories(callback4.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://photosfirstparty-pa.googleapis.com/v1/ntp/memories:read",
      R"(
        {
          "memory": [
            {
              "memoryMediaKey": "key1",
              "title": {
                "header": "Title 1",
                "subheader": "Something something 1"
              },
              "coverMediaKey": "coverKey1",
              "coverUrl": "https://photos.google.com/img/coverKey1"
            }
          ]
        }
      )",
      net::HTTP_OK,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);
  task_environment_.RunUntilQuit();

  EXPECT_EQ(1u, response1.size());
  EXPECT_EQ(1u, response2.size());
  EXPECT_EQ(1u, response3.size());
  EXPECT_EQ(1u, response4.size());
  EXPECT_EQ("Title 1", response1.at(0)->title);
  EXPECT_EQ("key1", response1.at(0)->id);
  EXPECT_EQ("Title 1", response2.at(0)->title);
  EXPECT_EQ("key1", response2.at(0)->id);
  EXPECT_EQ("Title 1", response3.at(0)->title);
  EXPECT_EQ("key1", response3.at(0)->id);
  EXPECT_EQ("Title 1", response4.at(0)->title);
  EXPECT_EQ("key1", response4.at(0)->id);
}

TEST_F(PhotosServiceTest, PassesNoDataOnAuthError) {
  auto quit_closure = task_environment_.QuitClosure();
  bool empty_response = false;
  base::MockCallback<PhotosService::GetMemoriesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<photos::mojom::MemoryPtr> memories) {
            empty_response = memories.empty();
            quit_closure.Run();
          }));

  service_->GetMemories(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::State::CONNECTION_FAILED));
  task_environment_.RunUntilQuit();

  EXPECT_TRUE(empty_response);
}

TEST_F(PhotosServiceTest, PassesNoDataOnNetError) {
  auto quit_closure = task_environment_.QuitClosure();
  bool empty_response = false;
  base::MockCallback<PhotosService::GetMemoriesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<photos::mojom::MemoryPtr> memories) {
            empty_response = memories.empty();
            quit_closure.Run();
          }));

  service_->GetMemories(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());

  ASSERT_EQ(1u, test_url_loader_factory_.pending_requests()->size());

  EXPECT_EQ(
      test_url_loader_factory_.pending_requests()
          ->at(0)
          .request.headers.ToString(),
      "Content-Type: application/json\r\nAuthorization: Bearer foo\r\n\r\n");
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://photosfirstparty-pa.googleapis.com/v1/ntp/memories:read",
      std::string(), net::HTTP_BAD_REQUEST,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);
  task_environment_.RunUntilQuit();

  EXPECT_TRUE(empty_response);
  ASSERT_EQ(1,
            histogram_tester_.GetBucketCount("NewTabPage.Modules.DataRequest",
                                             base::PersistentHash("photos")));
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Photos.DataRequest",
                   PhotosService::RequestResult::kError));
}

TEST_F(PhotosServiceTest, PassesNoDataOnEmptyResponse) {
  auto quit_closure = task_environment_.QuitClosure();
  bool empty_response = false;
  base::MockCallback<PhotosService::GetMemoriesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<photos::mojom::MemoryPtr> memories) {
            empty_response = memories.empty();
            quit_closure.Run();
          }));

  service_->GetMemories(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://photosfirstparty-pa.googleapis.com/v1/ntp/memories:read", "",
      net::HTTP_OK,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);
  task_environment_.RunUntilQuit();

  EXPECT_TRUE(empty_response);
}

TEST_F(PhotosServiceTest, PassesNoDataOnMissingItemKey) {
  auto quit_closure = task_environment_.QuitClosure();
  std::vector<photos::mojom::MemoryPtr> actual_memories;
  base::MockCallback<PhotosService::GetMemoriesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<photos::mojom::MemoryPtr> memories) {
            actual_memories = std::move(memories);
            quit_closure.Run();
          }));

  service_->GetMemories(callback.Get());

  identity_test_env.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "foo", base::Time());

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      "https://photosfirstparty-pa.googleapis.com/v1/ntp/memories:read",
      R"(
        {
          "memory": []
        }
      )",
      net::HTTP_OK,
      network::TestURLLoaderFactory::ResponseMatchFlags::kUrlMatchPrefix);
  task_environment_.RunUntilQuit();

  EXPECT_TRUE(actual_memories.empty());
  ASSERT_EQ(1,
            histogram_tester_.GetBucketCount("NewTabPage.Modules.DataRequest",
                                             base::PersistentHash("photos")));
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Photos.DataRequest",
                   PhotosService::RequestResult::kSuccess));
  ASSERT_EQ(1, histogram_tester_.GetBucketCount(
                   "NewTabPage.Photos.DataResponseCount", 0));
}

TEST_F(PhotosServiceTest, PassesNoDataIfDismissed) {
  bool empty_response = false;
  base::MockCallback<PhotosService::GetMemoriesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&empty_response](std::vector<photos::mojom::MemoryPtr> memories) {
            empty_response = memories.empty();
          }));

  prefs_.SetTime(PhotosService::kLastDismissedTimePrefName, base::Time::Now());
  service_->GetMemories(callback.Get());

  EXPECT_TRUE(empty_response);
}

TEST_F(PhotosServiceTest, SoftOptInNotShownWhenDisabled) {
  EXPECT_FALSE(service_->ShouldShowSoftOptOutButton());
  EXPECT_FALSE(service_->IsModuleSoftOptedOut());
}

TEST_F(PhotosServiceTest, DismissModule) {
  service_->DismissModule();
  EXPECT_EQ(base::Time::Now(),
            prefs_.GetTime(PhotosService::kLastDismissedTimePrefName));
}

TEST_F(PhotosServiceTest, RestoreModule) {
  service_->RestoreModule();
  EXPECT_EQ(base::Time(),
            prefs_.GetTime(PhotosService::kLastDismissedTimePrefName));
}

TEST_F(PhotosServiceTest, OptInShown) {
  EXPECT_TRUE(service_->ShouldShowOptInScreen());

  // If user does not accept opt-in, we should keep showing screen.
  service_->OnUserOptIn(false, nullptr, nullptr);
  EXPECT_TRUE(service_->ShouldShowOptInScreen());
  EXPECT_FALSE(prefs_.GetBoolean(PhotosService::kOptInAcknowledgedPrefName));

  // If user accept opt-in, we should stop showing screen.
  service_->OnUserOptIn(true, nullptr, nullptr);
  EXPECT_FALSE(service_->ShouldShowOptInScreen());
  EXPECT_TRUE(prefs_.GetBoolean(PhotosService::kOptInAcknowledgedPrefName));
}

TEST_F(PhotosServiceTest, DefaultOptInCardTitleReturned) {
  std::vector<photos::mojom::MemoryPtr> memory_list;

  auto recent_highlight = photos::mojom::Memory::New();
  recent_highlight->title = "Recent Highlights";
  memory_list.push_back(std::move(recent_highlight));

  auto n_years_ago = photos::mojom::Memory::New();
  n_years_ago->title = "6 Years Ago";
  memory_list.push_back(std::move(n_years_ago));

  auto mojo_memory = photos::mojom::Memory::New();
  mojo_memory->title = "Trip to India";
  memory_list.push_back(std::move(mojo_memory));

  EXPECT_EQ(
      l10n_util::GetStringUTF8(IDS_NTP_MODULES_PHOTOS_MEMORIES_WELCOME_TITLE),
      service_->GetOptInTitleText(std::move(memory_list)));
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Tests that the pref is cleared on signout.
// ChromeOS does not support signout.
TEST_F(PhotosServiceTest, ClearOnPrimaryAccountChange) {
  EXPECT_FALSE(prefs_.GetBoolean(PhotosService::kOptInAcknowledgedPrefName));

  // Opt-in current account
  service_->OnUserOptIn(true, NULL, NULL);
  EXPECT_TRUE(prefs_.GetBoolean(PhotosService::kOptInAcknowledgedPrefName));

  // Clear primary account which should trigger clearing the pref.
  identity_test_env.ClearPrimaryAccount();
  EXPECT_FALSE(prefs_.GetBoolean(PhotosService::kOptInAcknowledgedPrefName));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

class PhotosServiceFakeDataTest : public PhotosServiceTest {
 public:
  PhotosServiceFakeDataTest() {
    features_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpPhotosModule, {{"NtpPhotosModuleDataParam", "1"}});
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(PhotosServiceFakeDataTest, ReturnsFakeData) {
  auto quit_closure = task_environment_.QuitClosure();
  std::vector<photos::mojom::MemoryPtr> fake_memories;
  base::MockCallback<PhotosService::GetMemoriesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<photos::mojom::MemoryPtr> memories) {
            fake_memories = std::move(memories);
            quit_closure.Run();
          }));

  service_->GetMemories(callback.Get());
  task_environment_.RunUntilQuit();

  EXPECT_FALSE(fake_memories.empty());
}

class PhotosServiceSoftOptOutEnabledTest : public PhotosServiceTest {
 public:
  PhotosServiceSoftOptOutEnabledTest() {
    features_.InitAndEnableFeature(ntp_features::kNtpPhotosModuleSoftOptOut);
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(PhotosServiceSoftOptOutEnabledTest, PassesNoDataIfSoftOptedOut) {
  auto quit_closure = task_environment_.QuitClosure();
  bool empty_response = false;
  base::MockCallback<PhotosService::GetMemoriesCallback> callback;

  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<photos::mojom::MemoryPtr> memories) {
            empty_response = memories.empty();
            quit_closure.Run();
          }));

  prefs_.SetTime(PhotosService::kLastSoftOptedOutTimePrefName,
                 base::Time::Now());
  service_->GetMemories(callback.Get());
  task_environment_.RunUntilQuit();

  EXPECT_TRUE(empty_response);
}

TEST_F(PhotosServiceSoftOptOutEnabledTest, SoftOptOutShown) {
  EXPECT_TRUE(service_->ShouldShowSoftOptOutButton());
}

TEST_F(PhotosServiceSoftOptOutEnabledTest, SoftOptOutNotShownBeyondMaxOptOuts) {
  prefs_.SetInteger(PhotosService::kSoftOptOutCountPrefName,
                    PhotosService::kMaxSoftOptOuts);
  EXPECT_FALSE(service_->ShouldShowSoftOptOutButton());
}

TEST_F(PhotosServiceSoftOptOutEnabledTest, ModuleOptedOut) {
  prefs_.SetInteger(PhotosService::kSoftOptOutCountPrefName, 1);
  prefs_.SetTime(PhotosService::kLastSoftOptedOutTimePrefName,
                 base::Time::Now());
  EXPECT_TRUE(service_->IsModuleSoftOptedOut());
}

TEST_F(PhotosServiceSoftOptOutEnabledTest,
       PrefsUpdatedAppropriatelyWhenSoftOptedOut) {
  service_->SoftOptOut();
  EXPECT_EQ(prefs_.GetInteger(PhotosService::kSoftOptOutCountPrefName), 1);
  EXPECT_NE(prefs_.GetTime(PhotosService::kLastSoftOptedOutTimePrefName),
            base::Time());
}

TEST_F(PhotosServiceSoftOptOutEnabledTest,
       RestoreModuleUpdateSoftOptOutParams) {
  prefs_.SetTime(PhotosService::kLastSoftOptedOutTimePrefName,
                 base::Time::Now());
  prefs_.SetInteger(PhotosService::kSoftOptOutCountPrefName, 2);
  service_->RestoreModule();
  EXPECT_EQ(base::Time(),
            prefs_.GetTime(PhotosService::kLastSoftOptedOutTimePrefName));
  EXPECT_EQ(prefs_.GetInteger(PhotosService::kSoftOptOutCountPrefName), 1);
}

class PhotosServiceModulesRedesignedTest : public PhotosServiceTest {
 public:
  PhotosServiceModulesRedesignedTest() {
    features_.InitAndEnableFeature(ntp_features::kNtpModulesRedesigned);
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(PhotosServiceModulesRedesignedTest, IgnoresDismiss) {
  auto quit_closure = task_environment_.QuitClosure();
  bool passed_data = false;
  base::MockCallback<PhotosService::GetMemoriesCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(
          testing::Invoke([&](std::vector<photos::mojom::MemoryPtr> memories) {
            passed_data = !memories.empty();
            quit_closure.Run();
          }));
  identity_test_env.SetAutomaticIssueOfAccessTokens(/*grant=*/true);
  test_url_loader_factory_.AddResponse(
      "https://photosfirstparty-pa.googleapis.com/v1/ntp/memories:read",
      R"(
        {
          "memory": [
            {
              "memoryMediaKey": "key",
              "title": {
                "header": "Title",
                "subheader": "Something something"
              },
              "coverMediaKey": "coverKey",
              "coverUrl": "https://photos.google.com/img/coverKey"
            }
          ]
        }
      )");

  service_->DismissModule();
  service_->GetMemories(callback.Get());
  task_environment_.RunUntilQuit();

  EXPECT_TRUE(passed_data);
}

class PhotosServicePersonalizedOptInEnabledTest : public PhotosServiceTest {
 public:
  PhotosServicePersonalizedOptInEnabledTest() {
    features_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpPhotosModuleCustomizedOptInTitle,
        {{"NtpPhotosModuleOptInTitleParam",
          base::NumberToString(
              static_cast<int>(PhotosService::OptInCardTitle::kOptInpersonalizedTitle))}});
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(PhotosServicePersonalizedOptInEnabledTest, showsPersonalizedText) {
  std::vector<photos::mojom::MemoryPtr> memory_list;

  // RecentHighlights memory should be ignored.
  auto recent_highlight = photos::mojom::Memory::New();
  recent_highlight->title = "Recent Highlights";
  memory_list.push_back(std::move(recent_highlight));

  // N years ago memory should be ignored.
  auto n_years_ago = photos::mojom::Memory::New();
  n_years_ago->title = "6 Years Ago";
  memory_list.push_back(std::move(n_years_ago));

  auto mojo_memory = photos::mojom::Memory::New();
  mojo_memory->title = "Trip to India";
  memory_list.push_back(std::move(mojo_memory));

  std::string expected_string = "See \"Trip to India\" and other memories here";
  EXPECT_EQ(expected_string,
            service_->GetOptInTitleText(std::move(memory_list)));
}

TEST_F(PhotosServicePersonalizedOptInEnabledTest, IgnorelengthyTitles) {
  std::vector<photos::mojom::MemoryPtr> memory_list;

  // Memory with length > 20 - should be ignored.
  auto memory_with_long_title = photos::mojom::Memory::New();
  memory_with_long_title->title =
      "Look how fast they grow up blah blah blah blah";
  memory_list.push_back(std::move(memory_with_long_title));

  auto mojo_memory = photos::mojom::Memory::New();
  mojo_memory->title = "Trip to India";
  memory_list.push_back(std::move(mojo_memory));

  std::string expected_string = "See \"Trip to India\" and other memories here";
  EXPECT_EQ(expected_string,
            service_->GetOptInTitleText(std::move(memory_list)));
}

TEST_F(PhotosServicePersonalizedOptInEnabledTest,
       showsRHTitleWhenNoEligibleTitleFound) {
  std::vector<photos::mojom::MemoryPtr> memory_list;

  auto recent_highlight = photos::mojom::Memory::New();
  recent_highlight->title = "Recent Highlights";
  memory_list.push_back(std::move(recent_highlight));

  // N years ago memory should be ignored.
  auto n_years_ago = photos::mojom::Memory::New();
  n_years_ago->title = "6 Years Ago";
  memory_list.push_back(std::move(n_years_ago));

  // Memory with length > 20 - should be ignored.
  auto memory_with_long_title = photos::mojom::Memory::New();
  memory_with_long_title->title = "Look how fast they grow up blah blah blah";
  memory_list.push_back(std::move(memory_with_long_title));

  std::string expected_string = l10n_util::GetStringUTF8(
      IDS_NTP_MODULES_PHOTOS_MEMORIES_RH_WELCOME_TITLE);
  EXPECT_EQ(expected_string,
            service_->GetOptInTitleText(std::move(memory_list)));
}

TEST_F(PhotosServicePersonalizedOptInEnabledTest,
       showsDefaultTitleWhenNoEligibleTitleFoundAndRHAbsent) {
  std::vector<photos::mojom::MemoryPtr> memory_list;

  // N years ago memory should be ignored.
  auto n_years_ago = photos::mojom::Memory::New();
  n_years_ago->title = "6 Years Ago";
  memory_list.push_back(std::move(n_years_ago));

  // Memory with length > 20 - should be ignored.
  auto memory_with_long_title = photos::mojom::Memory::New();
  memory_with_long_title->title =
      "Look how fast they grow up and blah blah blah blah";
  memory_list.push_back(std::move(memory_with_long_title));

  std::string expected_string =
      l10n_util::GetStringUTF8(IDS_NTP_MODULES_PHOTOS_MEMORIES_WELCOME_TITLE);
  EXPECT_EQ(expected_string,
            service_->GetOptInTitleText(std::move(memory_list)));
}

class PhotosServiceRHTitleEnabledTest : public PhotosServiceTest {
 public:
  PhotosServiceRHTitleEnabledTest() {
    features_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpPhotosModuleCustomizedOptInTitle,
        {{"NtpPhotosModuleOptInTitleParam",
          base::NumberToString(
              static_cast<int>(PhotosService::OptInCardTitle::kOptInRHTitle))}});
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(PhotosServiceRHTitleEnabledTest, showsRHTitle) {
  std::vector<photos::mojom::MemoryPtr> memory_list;

  auto recent_highlight = photos::mojom::Memory::New();
  recent_highlight->title = "Recent Highlights";
  memory_list.push_back(std::move(recent_highlight));

  auto n_years_ago = photos::mojom::Memory::New();
  n_years_ago->title = "6 Years Ago";
  memory_list.push_back(std::move(n_years_ago));

  auto mojo_memory = photos::mojom::Memory::New();
  mojo_memory->title = "Trip to India";
  memory_list.push_back(std::move(mojo_memory));

  std::string expected_string = l10n_util::GetStringUTF8(
      IDS_NTP_MODULES_PHOTOS_MEMORIES_RH_WELCOME_TITLE);
  EXPECT_EQ(expected_string,
            service_->GetOptInTitleText(std::move(memory_list)));
}

class PhotosServiceFavoritePeopleTitleEnabledTest : public PhotosServiceTest {
 public:
  PhotosServiceFavoritePeopleTitleEnabledTest() {
    features_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpPhotosModuleCustomizedOptInTitle,
        {{"NtpPhotosModuleOptInTitleParam",
          base::NumberToString(
              static_cast<int>(PhotosService::OptInCardTitle::kOptInFavoritesTitle))}});
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(PhotosServiceFavoritePeopleTitleEnabledTest, showsRHTitle) {
  std::vector<photos::mojom::MemoryPtr> memory_list;

  auto recent_highlight = photos::mojom::Memory::New();
  recent_highlight->title = "Recent Highlights";
  memory_list.push_back(std::move(recent_highlight));

  auto n_years_ago = photos::mojom::Memory::New();
  n_years_ago->title = "6 Years Ago";
  memory_list.push_back(std::move(n_years_ago));

  auto mojo_memory = photos::mojom::Memory::New();
  mojo_memory->title = "Trip to India";
  memory_list.push_back(std::move(mojo_memory));

  std::string expected_string = l10n_util::GetStringUTF8(
      IDS_NTP_MODULES_PHOTOS_MEMORIES_FAVORITE_PEOPLE_WELCOME_TITLE);
  EXPECT_EQ(expected_string,
            service_->GetOptInTitleText(std::move(memory_list)));
}

class PhotosServiceTripTitleEnabledTest : public PhotosServiceTest {
 public:
  PhotosServiceTripTitleEnabledTest() {
    features_.InitAndEnableFeatureWithParameters(
        ntp_features::kNtpPhotosModuleCustomizedOptInTitle,
        {{"NtpPhotosModuleOptInTitleParam",
          base::NumberToString(static_cast<int>(
              PhotosService::OptInCardTitle::kOptInTripsTitle))}});
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_F(PhotosServiceTripTitleEnabledTest, showsTripsTitle) {
  std::vector<photos::mojom::MemoryPtr> memory_list;

  auto recent_highlight = photos::mojom::Memory::New();
  recent_highlight->title = "Recent Highlights";
  memory_list.push_back(std::move(recent_highlight));

  auto n_years_ago = photos::mojom::Memory::New();
  n_years_ago->title = "6 Years Ago";
  memory_list.push_back(std::move(n_years_ago));

  auto mojo_memory = photos::mojom::Memory::New();
  mojo_memory->title = "Trip to India";
  memory_list.push_back(std::move(mojo_memory));

  std::string expected_string = l10n_util::GetStringUTF8(
      IDS_NTP_MODULES_PHOTOS_MEMORIES_TRIPS_WELCOME_TITLE);
  EXPECT_EQ(expected_string,
            service_->GetOptInTitleText(std::move(memory_list)));
}
