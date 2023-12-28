// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_fetch/background_fetch_delegate_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const char kUserInitiatedAbort[] = "UserInitiatedAbort";

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
