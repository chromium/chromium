// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/prediction_model_download_manager.h"

#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/download/public/background_service/test/mock_download_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

using ::testing::_;
using ::testing::Eq;
using ::testing::SaveArg;

class PredictionModelDownloadManagerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PredictionModelDownloadManagerTest() = default;
  ~PredictionModelDownloadManagerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    mock_download_service_ = static_cast<download::test::MockDownloadService*>(
        DownloadServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile()->GetProfileKey(),
            base::BindRepeating([](SimpleFactoryKey* key)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<download::test::MockDownloadService>();
            })));
    download_manager_ =
        std::make_unique<PredictionModelDownloadManager>(profile());
  }

  void TearDown() override {
    download_manager_.reset();
    mock_download_service_ = nullptr;

    ChromeRenderViewHostTestHarness::TearDown();
  }

  PredictionModelDownloadManager* download_manager() {
    return download_manager_.get();
  }

  download::test::MockDownloadService* download_service() {
    return mock_download_service_;
  }

 protected:
  void SetDownloadServiceReady(const std::set<std::string>& pending_guids,
                               const std::set<std::string>& successful_guids) {
    std::map<std::string, base::FilePath> success_map;
    for (const auto& guid : successful_guids) {
      success_map.emplace(guid, temp_dir_.GetPath());
    }
    download_manager()->OnDownloadServiceReady(pending_guids, success_map);
  }

  void SetDownloadServiceUnavailable() {
    download_manager()->OnDownloadServiceUnavailable();
  }

  void SetDownloadSucceeded(const std::string& guid) {
    download_manager()->OnDownloadSucceeded(guid, temp_dir_.GetPath());
  }

  void SetDownloadFailed(const std::string& guid) {
    download_manager()->OnDownloadFailed(guid);
  }

 private:
  base::ScopedTempDir temp_dir_;
  download::test::MockDownloadService* mock_download_service_;
  std::unique_ptr<PredictionModelDownloadManager> download_manager_;
};

TEST_F(PredictionModelDownloadManagerTest, DownloadServiceReadyPersistsGuids) {
  SetDownloadServiceReady({"pending1", "pending2", "pending3"},
                          {"success1", "success2", "success3"});

  // Should only persist and thus cancel the pending ones.
  EXPECT_CALL(*download_service(), CancelDownload(Eq("pending1")));
  EXPECT_CALL(*download_service(), CancelDownload(Eq("pending2")));
  EXPECT_CALL(*download_service(), CancelDownload(Eq("pending3")));
  download_manager()->CancelAllPendingDownloads();
}

TEST_F(PredictionModelDownloadManagerTest, StartDownload) {
  download::DownloadParams download_params;
  EXPECT_CALL(*download_service(), StartDownload(_))
      .WillOnce(SaveArg<0>(&download_params));
  download_manager()->StartDownload(GURL("someurl"));

  // Validate parameters - basically that we attach the correct client, just do
  // a passthrough of the URL, and attach the API key.
  EXPECT_EQ(download_params.client,
            download::DownloadClient::OPTIMIZATION_GUIDE_PREDICTION_MODELS);
  EXPECT_EQ(download_params.request_params.url, GURL("someurl"));
  EXPECT_EQ(download_params.request_params.method, "GET");
  EXPECT_TRUE(download_params.request_params.request_headers.HasHeader(
      "X-Goog-Api-Key"));

  // Now invoke start callback.
  std::move(download_params.callback)
      .Run("someguid", download::DownloadParams::StartResult::ACCEPTED);

  // Now cancel all downloads to ensure that callback persisted pending GUID.
  EXPECT_CALL(*download_service(), CancelDownload(Eq("someguid")));
  download_manager()->CancelAllPendingDownloads();
}

TEST_F(PredictionModelDownloadManagerTest, StartDownloadFailedToSchedule) {
  download::DownloadParams download_params;
  EXPECT_CALL(*download_service(), StartDownload(_))
      .WillOnce(SaveArg<0>(&download_params));
  download_manager()->StartDownload(GURL("someurl"));

  // Now invoke start callback.
  std::move(download_params.callback)
      .Run("someguid", download::DownloadParams::StartResult::INTERNAL_ERROR);

  // Now cancel all downloads to ensure that bad GUID was not accepted.
  EXPECT_CALL(*download_service(), CancelDownload(_)).Times(0);
  download_manager()->CancelAllPendingDownloads();
}

TEST_F(PredictionModelDownloadManagerTest, IsAvailableForDownloads) {
  EXPECT_TRUE(download_manager()->IsAvailableForDownloads());

  SetDownloadServiceUnavailable();

  EXPECT_FALSE(download_manager()->IsAvailableForDownloads());
}

TEST_F(PredictionModelDownloadManagerTest,
       SuccessfulDownloadShouldNoLongerBeTracked) {
  SetDownloadServiceReady({"pending1", "pending2", "pending3"},
                          /*successful_guids=*/{});

  SetDownloadSucceeded("pending1");

  // Should only persist and thus cancel the pending ones.
  EXPECT_CALL(*download_service(), CancelDownload(Eq("pending2")));
  EXPECT_CALL(*download_service(), CancelDownload(Eq("pending3")));
  download_manager()->CancelAllPendingDownloads();
}

TEST_F(PredictionModelDownloadManagerTest,
       FailedDownloadShouldNoLongerBeTracked) {
  SetDownloadServiceReady({"pending1", "pending2", "pending3"},
                          /*successful_guids=*/{});

  SetDownloadSucceeded("pending2");

  // Should only persist and thus cancel the pending ones.
  EXPECT_CALL(*download_service(), CancelDownload(Eq("pending1")));
  EXPECT_CALL(*download_service(), CancelDownload(Eq("pending3")));
  download_manager()->CancelAllPendingDownloads();
}

}  // namespace optimization_guide
