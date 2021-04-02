// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/prediction_model_download_manager.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model_download_observer.h"
#include "components/download/public/background_service/test/mock_download_service.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/services/unzip/content/unzip_service.h"
#include "components/services/unzip/in_process_unzipper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"

namespace optimization_guide {

using ::testing::_;
using ::testing::Eq;
using ::testing::SaveArg;

class TestPredictionModelDownloadObserver
    : public PredictionModelDownloadObserver {
 public:
  TestPredictionModelDownloadObserver() = default;
  ~TestPredictionModelDownloadObserver() override = default;

  void OnModelReady(const proto::PredictionModel& model) override {
    last_ready_model_ = model;
  }

  base::Optional<proto::PredictionModel> last_ready_model() const {
    return last_ready_model_;
  }

 private:
  base::Optional<proto::PredictionModel> last_ready_model_;
};

enum class PredictionModelDownloadFileStatus {
  kVerifiedCrxWithGoodModelFiles,
  kVerifiedCrxWithNoFiles,
  kVerifiedCrxWithInvalidPublisher,
  kVerifiedCrxWithBadModelInfoFile,
  kVerifiedCrxWithInvalidModelInfo,
  kVerfiedCrxWithValidModelInfoNoModelFile,
  kUnverifiedFile,
};

class PredictionModelDownloadManagerTest : public testing::Test {
 public:
  PredictionModelDownloadManagerTest() = default;
  ~PredictionModelDownloadManagerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    mock_download_service_ =
        std::make_unique<download::test::MockDownloadService>();
    download_manager_ = std::make_unique<PredictionModelDownloadManager>(
        mock_download_service_.get(), temp_dir_.GetPath(),
        task_environment_.GetMainThreadTaskRunner());

    unzip::SetUnzipperLaunchOverrideForTesting(
        base::BindRepeating(&unzip::LaunchInProcessUnzipper));
  }

  void TearDown() override {
    download_manager_.reset();
    mock_download_service_ = nullptr;
  }

  PredictionModelDownloadManager* download_manager() {
    return download_manager_.get();
  }

  download::test::MockDownloadService* download_service() {
    return mock_download_service_.get();
  }

 protected:
  void SetDownloadServiceReady(
      const std::set<std::string>& pending_guids,
      const std::map<std::string, PredictionModelDownloadFileStatus>&
          successful_guids) {
    std::map<std::string, base::FilePath> success_map;
    for (const auto& guid_and_status : successful_guids) {
      success_map.emplace(
          guid_and_status.first,
          GetFilePathForDownloadFileStatus(guid_and_status.second));
    }
    download_manager()->OnDownloadServiceReady(pending_guids, success_map);
  }

  void SetDownloadServiceUnavailable() {
    download_manager()->OnDownloadServiceUnavailable();
  }

  void SetDownloadSucceeded(const std::string& guid,
                            PredictionModelDownloadFileStatus file_status) {
    WriteFileForStatus(file_status);
    download_manager()->OnDownloadSucceeded(
        guid, GetFilePathForDownloadFileStatus(file_status));
  }

  void SetDownloadFailed(const std::string& guid) {
    download_manager()->OnDownloadFailed(guid);
  }

  void RunUntilIdle() {
    task_environment_.RunUntilIdle();

    // Wait for all delayed tasks to finish.
    base::RunLoop run_loop;
    base::ThreadPoolInstance::Get()->FlushAsyncForTesting(
        run_loop.QuitClosure());
    run_loop.Run();
  }

  base::FilePath GetFilePathForDownloadFileStatus(
      PredictionModelDownloadFileStatus file_status) {
    base::FilePath path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
    switch (file_status) {
      case PredictionModelDownloadFileStatus::kUnverifiedFile:
        return temp_dir_.GetPath().AppendASCII("unverified.crx3");
      case PredictionModelDownloadFileStatus::kVerifiedCrxWithInvalidPublisher:
        return temp_dir_.GetPath().AppendASCII("invalidpublisher.crx3");
      case PredictionModelDownloadFileStatus::kVerifiedCrxWithNoFiles:
        return temp_dir_.GetPath().AppendASCII("nofiles.crx3");
      case PredictionModelDownloadFileStatus::kVerifiedCrxWithBadModelInfoFile:
        return temp_dir_.GetPath().AppendASCII("badmodelinfo.crx3");
      case PredictionModelDownloadFileStatus::kVerifiedCrxWithInvalidModelInfo:
        return temp_dir_.GetPath().AppendASCII("invalidmodelinfo.crx3");
      case PredictionModelDownloadFileStatus::
          kVerfiedCrxWithValidModelInfoNoModelFile:
        return temp_dir_.GetPath().AppendASCII("nomodel.crx3");
      case PredictionModelDownloadFileStatus::kVerifiedCrxWithGoodModelFiles:
        return temp_dir_.GetPath().AppendASCII("good.crx3");
    }
  }

  void TurnOffDownloadVerification() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableModelDownloadVerificationForTesting);
  }

  // Retries until the path has been deleted or until all handles to |path| have
  // been closed. Returns whether |path| has been deleted.
  //
  // See crbug/1156112#c1 for suggested mitigation steps.
  bool HasPathBeenDeleted(const base::FilePath& path) {
    while (true) {
      RunUntilIdle();

      bool path_exists = base::PathExists(path);
      if (!path_exists)
        return true;

      // Retry if the last file error is access denied since it's likely that
      // the file is in the process of being deleted.
    }
  }

 private:
  void WriteFileForStatus(PredictionModelDownloadFileStatus status) {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    if (status == PredictionModelDownloadFileStatus::
                      kVerifiedCrxWithInvalidPublisher ||
        status == PredictionModelDownloadFileStatus::kUnverifiedFile) {
      base::FilePath crx_file_source_dir =
          source_root_dir.AppendASCII("components")
              .AppendASCII("test")
              .AppendASCII("data")
              .AppendASCII("crx_file");
      std::string crx_file =
          status == PredictionModelDownloadFileStatus::kUnverifiedFile
              ? "unsigned.crx3"
              : "valid_publisher.crx3";  // Despite name, wrong publisher.
      ASSERT_TRUE(base::CopyFile(crx_file_source_dir.AppendASCII(crx_file),
                                 GetFilePathForDownloadFileStatus(status)));
      return;
    }

    if (status == PredictionModelDownloadFileStatus::kVerifiedCrxWithNoFiles) {
      base::FilePath invalid_crx_model = source_root_dir.AppendASCII("chrome")
                                             .AppendASCII("test")
                                             .AppendASCII("data")
                                             .AppendASCII("optimization_guide")
                                             .AppendASCII("invalid_model.crx3");
      ASSERT_TRUE(base::CopyFile(invalid_crx_model,
                                 GetFilePathForDownloadFileStatus(status)));
      return;
    }

    base::FilePath zip_dir = temp_dir_.GetPath().AppendASCII("zip_dir");
    ASSERT_TRUE(base::CreateDirectory(zip_dir));
    if (status ==
        PredictionModelDownloadFileStatus::kVerifiedCrxWithBadModelInfoFile) {
      base::WriteFile(zip_dir.AppendASCII("model-info.pb"), "boo", 3);
    } else {
      proto::ModelInfo model_info;
      model_info.set_optimization_target(
          proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
      model_info.set_version(123);
      if (status ==
          PredictionModelDownloadFileStatus::kVerifiedCrxWithInvalidModelInfo) {
        model_info.clear_version();
      }

      std::string serialized_model_info;
      ASSERT_TRUE(model_info.SerializeToString(&serialized_model_info));
      ASSERT_EQ(static_cast<int32_t>(serialized_model_info.length()),
                base::WriteFile(zip_dir.AppendASCII("model-info.pb"),
                                serialized_model_info.data(),
                                serialized_model_info.length()));
      if (status ==
          PredictionModelDownloadFileStatus::kVerifiedCrxWithGoodModelFiles) {
        base::WriteFile(zip_dir.AppendASCII("model.tflite"), "model", 5);
      }
    }
    ASSERT_TRUE(
        zip::Zip(zip_dir, GetFilePathForDownloadFileStatus(status), true));
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<download::test::MockDownloadService> mock_download_service_;
  std::unique_ptr<PredictionModelDownloadManager> download_manager_;
};

TEST_F(PredictionModelDownloadManagerTest, DownloadServiceReadyPersistsGuids) {
  base::HistogramTester histogram_tester;

  SetDownloadServiceReady(
      {"pending1", "pending2", "pending3"},
      {{"success1", PredictionModelDownloadFileStatus::kUnverifiedFile},
       {"success2", PredictionModelDownloadFileStatus::kUnverifiedFile},
       {"success3", PredictionModelDownloadFileStatus::kUnverifiedFile}});
  RunUntilIdle();

  // Should only persist and thus cancel the pending ones.
  EXPECT_CALL(*download_service(), CancelDownload(Eq("pending1")));
  EXPECT_CALL(*download_service(), CancelDownload(Eq("pending2")));
  EXPECT_CALL(*download_service(), CancelDownload(Eq("pending3")));
  download_manager()->CancelAllPendingDownloads();

  // The successful downloads should not trigger us to do anything with them.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadSucceeded", 0);
}

TEST_F(PredictionModelDownloadManagerTest, StartDownloadRestrictedDownloading) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {optimization_guide::features::kOptimizationGuideModelDownloading,
           {{"unrestricted_model_downloading", "false"}}},
      },
      /*disabled_features=*/{});

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
  EXPECT_EQ(download_params.scheduling_params.priority,
            download::SchedulingParams::Priority::NORMAL);
  EXPECT_EQ(
      download_params.scheduling_params.battery_requirements,
      download::SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE);
  EXPECT_EQ(download_params.scheduling_params.network_requirements,
            download::SchedulingParams::NetworkRequirements::NONE);

  // Now invoke start callback.
  std::move(download_params.callback)
      .Run("someguid", download::DownloadParams::StartResult::ACCEPTED);

  // Now cancel all downloads to ensure that callback persisted pending GUID.
  EXPECT_CALL(*download_service(), CancelDownload(Eq("someguid")));
  download_manager()->CancelAllPendingDownloads();
}

TEST_F(PredictionModelDownloadManagerTest,
       StartDownloadUnrestrictedDownloading) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {optimization_guide::features::kOptimizationGuideModelDownloading,
           {{"unrestricted_model_downloading", "true"}}},
      },
      /*disabled_features=*/{});

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
  EXPECT_EQ(download_params.scheduling_params.priority,
            download::SchedulingParams::Priority::HIGH);
  EXPECT_EQ(
      download_params.scheduling_params.battery_requirements,
      download::SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE);
  EXPECT_EQ(download_params.scheduling_params.network_requirements,
            download::SchedulingParams::NetworkRequirements::NONE);

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
  base::HistogramTester histogram_tester;

  SetDownloadServiceReady({"pending1", "pending2", "pending3"},
                          /*successful_guids=*/{});

  SetDownloadSucceeded("pending1",
                       PredictionModelDownloadFileStatus::kUnverifiedFile);
  RunUntilIdle();

  // Should only persist and thus cancel the pending ones.
  EXPECT_CALL(*download_service(), CancelDownload(Eq("pending2")));
  EXPECT_CALL(*download_service(), CancelDownload(Eq("pending3")));
  download_manager()->CancelAllPendingDownloads();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadSucceeded",
      true, 1);
}

TEST_F(PredictionModelDownloadManagerTest,
       FailedDownloadShouldNoLongerBeTracked) {
  base::HistogramTester histogram_tester;

  SetDownloadServiceReady({"pending1", "pending2", "pending3"},
                          /*successful_guids=*/{});

  SetDownloadFailed("pending2");

  // Should only persist and thus cancel the pending ones.
  EXPECT_CALL(*download_service(), CancelDownload(Eq("pending1")));
  EXPECT_CALL(*download_service(), CancelDownload(Eq("pending3")));
  download_manager()->CancelAllPendingDownloads();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadSucceeded",
      false, 1);
}

TEST_F(PredictionModelDownloadManagerTest, UnverifiedFileShouldDeleteTempFile) {
  base::HistogramTester histogram_tester;

  TestPredictionModelDownloadObserver observer;
  download_manager()->AddObserver(&observer);

  SetDownloadSucceeded("model",
                       PredictionModelDownloadFileStatus::kUnverifiedFile);
  RunUntilIdle();

  EXPECT_FALSE(observer.last_ready_model().has_value());
  EXPECT_TRUE(HasPathBeenDeleted(GetFilePathForDownloadFileStatus(
      PredictionModelDownloadFileStatus::kUnverifiedFile)));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager."
      "DownloadStatus",
      PredictionModelDownloadStatus::kFailedCrxVerification, 1);
}

// TODO(crbug.com/1156112): Flaky on Windows.
#if defined(OS_WIN)
#define MAYBE_VerifiedCrxWithInvalidPublisherShouldDeleteTempFile \
  DISABLED_VerifiedCrxWithInvalidPublisherShouldDeleteTempFile
#else
#define MAYBE_VerifiedCrxWithInvalidPublisherShouldDeleteTempFile \
  VerifiedCrxWithInvalidPublisherShouldDeleteTempFile
#endif
TEST_F(PredictionModelDownloadManagerTest,
       MAYBE_VerifiedCrxWithInvalidPublisherShouldDeleteTempFile) {
  base::HistogramTester histogram_tester;

  TestPredictionModelDownloadObserver observer;
  download_manager()->AddObserver(&observer);

  SetDownloadSucceeded(
      "model",
      PredictionModelDownloadFileStatus::kVerifiedCrxWithInvalidPublisher);
  RunUntilIdle();

  EXPECT_FALSE(observer.last_ready_model().has_value());
  EXPECT_TRUE(HasPathBeenDeleted(GetFilePathForDownloadFileStatus(
      PredictionModelDownloadFileStatus::kVerifiedCrxWithInvalidPublisher)));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager."
      "DownloadStatus",
      PredictionModelDownloadStatus::kFailedCrxInvalidPublisher, 1);
}

TEST_F(PredictionModelDownloadManagerTest,
       VerifiedCrxWithNoFilesShouldDeleteTempFile) {
  base::HistogramTester histogram_tester;

  TestPredictionModelDownloadObserver observer;
  download_manager()->AddObserver(&observer);

  SetDownloadSucceeded(
      "model", PredictionModelDownloadFileStatus::kVerifiedCrxWithNoFiles);
  RunUntilIdle();

  EXPECT_FALSE(observer.last_ready_model().has_value());
  EXPECT_TRUE(HasPathBeenDeleted(GetFilePathForDownloadFileStatus(
      PredictionModelDownloadFileStatus::kVerifiedCrxWithNoFiles)));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager."
      "DownloadStatus",
      PredictionModelDownloadStatus::kFailedModelInfoFileRead, 1);
}

TEST_F(PredictionModelDownloadManagerTest,
       VerifiedCrxWithBadModelInfoFileShouldDeleteTempFile) {
  base::HistogramTester histogram_tester;

  TestPredictionModelDownloadObserver observer;
  download_manager()->AddObserver(&observer);
  TurnOffDownloadVerification();

  SetDownloadSucceeded(
      "model",
      PredictionModelDownloadFileStatus::kVerifiedCrxWithBadModelInfoFile);
  RunUntilIdle();

  EXPECT_FALSE(observer.last_ready_model().has_value());
  EXPECT_TRUE(HasPathBeenDeleted(GetFilePathForDownloadFileStatus(
      PredictionModelDownloadFileStatus::kVerifiedCrxWithBadModelInfoFile)));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager."
      "DownloadStatus",
      PredictionModelDownloadStatus::kFailedModelInfoParsing, 1);
}

TEST_F(PredictionModelDownloadManagerTest,
       VerifiedCrxWithInvalidModelInfoShouldDeleteTempFile) {
  base::HistogramTester histogram_tester;

  TestPredictionModelDownloadObserver observer;
  download_manager()->AddObserver(&observer);
  TurnOffDownloadVerification();

  SetDownloadSucceeded(
      "model",
      PredictionModelDownloadFileStatus::kVerifiedCrxWithInvalidModelInfo);
  RunUntilIdle();

  EXPECT_FALSE(observer.last_ready_model().has_value());
  EXPECT_TRUE(HasPathBeenDeleted(GetFilePathForDownloadFileStatus(
      PredictionModelDownloadFileStatus::kVerifiedCrxWithInvalidModelInfo)));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager."
      "DownloadStatus",
      PredictionModelDownloadStatus::kFailedModelInfoInvalid, 1);
}

TEST_F(PredictionModelDownloadManagerTest,
       VerifiedCrxWithValidModelInfoFileButNoModelFileShouldDeleteTempFile) {
  base::HistogramTester histogram_tester;

  TestPredictionModelDownloadObserver observer;
  download_manager()->AddObserver(&observer);
  TurnOffDownloadVerification();

  SetDownloadSucceeded("model", PredictionModelDownloadFileStatus::
                                    kVerfiedCrxWithValidModelInfoNoModelFile);
  RunUntilIdle();

  EXPECT_FALSE(observer.last_ready_model().has_value());
  EXPECT_TRUE(HasPathBeenDeleted(GetFilePathForDownloadFileStatus(
      PredictionModelDownloadFileStatus::
          kVerfiedCrxWithValidModelInfoNoModelFile)));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager."
      "DownloadStatus",
      PredictionModelDownloadStatus::kFailedModelFileNotFound, 1);
}

// TODO(crbug.com/1156112): Flaky on Windows.
#if defined(OS_WIN)
#define MAYBE_VerifiedCrxWithGoodModelFilesShouldDeleteDownloadFileButHaveContentExtracted \
  DISABLED_VerifiedCrxWithGoodModelFilesShouldDeleteDownloadFileButHaveContentExtracted
#else
#define MAYBE_VerifiedCrxWithGoodModelFilesShouldDeleteDownloadFileButHaveContentExtracted \
  VerifiedCrxWithGoodModelFilesShouldDeleteDownloadFileButHaveContentExtracted
#endif
TEST_F(
    PredictionModelDownloadManagerTest,
    MAYBE_VerifiedCrxWithGoodModelFilesShouldDeleteDownloadFileButHaveContentExtracted) {
  base::HistogramTester histogram_tester;

  TestPredictionModelDownloadObserver observer;
  download_manager()->AddObserver(&observer);
  TurnOffDownloadVerification();

  SetDownloadSucceeded(
      "modelfile",
      PredictionModelDownloadFileStatus::kVerifiedCrxWithGoodModelFiles);
  RunUntilIdle();

  EXPECT_TRUE(observer.last_ready_model().has_value());
  EXPECT_EQ(observer.last_ready_model()->model_info().optimization_target(),
            proto::OptimizationTarget::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  EXPECT_EQ(observer.last_ready_model()->model_info().version(), 123);
  EXPECT_EQ(
      GetFilePathFromPredictionModel(observer.last_ready_model().value())
          .value()
          .BaseName()
          .value(),
      FILE_PATH_LITERAL("OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD_123.tflite"));
  // Downloaded file should still be deleted.
  EXPECT_TRUE(HasPathBeenDeleted(GetFilePathForDownloadFileStatus(
      PredictionModelDownloadFileStatus::kVerifiedCrxWithGoodModelFiles)));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager."
      "DownloadStatus",
      PredictionModelDownloadStatus::kSuccess, 1);
}

}  // namespace optimization_guide
