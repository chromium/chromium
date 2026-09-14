// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_fetch/background_fetch_delegate_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/background_fetch_description.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/test/browser_task_environment.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const char kUserInitiatedAbort[] = "UserInitiatedAbort";

class MockBackgroundFetchDelegateClient
    : public content::BackgroundFetchDelegate::Client {
 public:
  MOCK_METHOD(void,
              OnJobCancelled,
              (const std::string& job_unique_id,
               const std::string& download_guid,
               blink::mojom::BackgroundFetchFailureReason reason_to_abort),
              (override));
  MOCK_METHOD(void,
              OnDownloadStarted,
              (const std::string& job_unique_id,
               const std::string& download_guid,
               std::unique_ptr<content::BackgroundFetchResponse> response),
              (override));
  MOCK_METHOD(void,
              OnDownloadUpdated,
              (const std::string& job_unique_id,
               const std::string& download_guid,
               uint64_t bytes_uploaded,
               uint64_t bytes_downloaded),
              (override));
  MOCK_METHOD(void,
              OnDownloadComplete,
              (const std::string& job_unique_id,
               const std::string& download_guid,
               std::unique_ptr<content::BackgroundFetchResult> result),
              (override));
  MOCK_METHOD(void,
              OnUIActivated,
              (const std::string& job_unique_id),
              (override));
  MOCK_METHOD(void,
              OnUIUpdated,
              (const std::string& job_unique_id),
              (override));
  MOCK_METHOD(void,
              GetUploadData,
              (const std::string& job_unique_id,
               const std::string& download_guid,
               GetUploadDataCallback callback),
              (override));

  base::WeakPtr<MockBackgroundFetchDelegateClient> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockBackgroundFetchDelegateClient> weak_ptr_factory_{
      this};
};

}  // namespace

class BackgroundFetchDelegateImplTest : public testing::Test {
 public:
  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();

    recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    delegate_ = static_cast<BackgroundFetchDelegateImpl*>(
        profile_->GetBackgroundFetchDelegate());

    // Add |kOriginUrl| to |profile_|'s history so the UKM background
    // recording conditions are met.
    auto* history_service = HistoryServiceFactory::GetForProfile(
        profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);
    history_service->AddPage(kOriginUrl, base::Time::Now(),
                             history::SOURCE_BROWSED);
  }

 protected:
  // This is used to specify the main thread type of the tests as the UI
  // thread.
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> recorder_;
  std::unique_ptr<TestingProfile> profile_;

  // Can't outlive `profile_` which owns it.
  raw_ptr<BackgroundFetchDelegateImpl> delegate_;

  const GURL kOriginUrl{"https://example.com/"};
};

TEST_F(BackgroundFetchDelegateImplTest, RecordUkmEvent) {
  url::Origin origin = url::Origin::Create(kOriginUrl);

  {
    std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>
        entries = recorder_->GetEntriesByName(
            ukm::builders::BackgroundFetchDeletingRegistration::kEntryName);
    EXPECT_EQ(entries.size(), 0u);
  }

  base::RunLoop run_loop;
  recorder_->SetOnAddEntryCallback(
      ukm::builders::BackgroundFetchDeletingRegistration::kEntryName,
      run_loop.QuitClosure());
  delegate_->RecordBackgroundFetchDeletingRegistrationUkmEvent(
      origin, /* user_initiated_abort= */ true);
  run_loop.Run();

  {
    std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>
        entries = recorder_->GetEntriesByName(
            ukm::builders::BackgroundFetchDeletingRegistration::kEntryName);
    ASSERT_EQ(entries.size(), 1u);
    auto* entry = recorder_
                      ->GetEntriesByName(
                          ukm::builders::BackgroundFetchDeletingRegistration::
                              kEntryName)[0]
                      .get();
    recorder_->ExpectEntryMetric(entry, kUserInitiatedAbort, 1);
  }
}

TEST_F(BackgroundFetchDelegateImplTest, FailFetchCallsOnJobCancelled) {
  const std::string kJobId = "job_id";
  const std::string kDownloadGuid = "download_guid";

  MockBackgroundFetchDelegateClient client;
  auto description = std::make_unique<content::BackgroundFetchDescription>(
      kJobId, url::Origin::Create(kOriginUrl), /*title=*/"Title",
      /*icon=*/SkBitmap(), /*completed_requests=*/0, /*total_requests=*/1,
      /*downloaded_bytes=*/0, /*uploaded_bytes=*/0,
      /*download_total_bytes=*/100, /*upload_total_bytes=*/0,
      /*outstanding_guids=*/std::vector<std::string>{kDownloadGuid},
      /*start_paused=*/false, /*isolation_info=*/std::nullopt);

  delegate_->CreateDownloadJob(client.GetWeakPtr(), std::move(description));

  EXPECT_CALL(
      client,
      OnJobCancelled(
          kJobId, kDownloadGuid,
          blink::mojom::BackgroundFetchFailureReason::DOWNLOAD_TOTAL_EXCEEDED));

  delegate_->OnDownloadUpdated(kDownloadGuid, /*bytes_uploaded=*/0,
                               /*bytes_downloaded=*/101);
}
