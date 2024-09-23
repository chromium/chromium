// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "ash/annotator/annotator_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/projector/model/projector_session_impl.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/projector/projector_metadata_controller.h"
#include "ash/projector/projector_metrics.h"
#include "ash/projector/test/mock_projector_metadata_controller.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/public/cpp/projector/speech_recognition_availability.h"
#include "ash/public/cpp/test/mock_projector_client.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_container.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/annotator/test/mock_annotator_client.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/safe_base_name.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/audio_node.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {
namespace {
using testing::_;
using testing::ElementsAre;

struct AudioNodeInfo {
  bool is_input;
  uint64_t id;
  const char* const device_name;
  const char* const type;
  const char* const name;
};

constexpr char kProjectorCreationFlowHistogramName[] =
    "Ash.Projector.CreationFlow.ClamshellMode";

constexpr char kProjectorTranscriptsCountHistogramName[] =
    "Ash.Projector.TranscriptsCount.ClamshellMode";

constexpr char kSpeechRecognitionEndStateOnDevice[] =
    "Ash.Projector.SpeechRecognitionEndState.OnDevice";

constexpr char kSpeechRecognitionEndStateServerBased[] =
    "Ash.Projector.SpeechRecognitionEndState.ServerBased";

constexpr char kMetadataFileName[] = "MyScreencast";
constexpr char kProjectorV2Extension[] = "screencast";

void NotifyControllerForFinalSpeechResult(ProjectorControllerImpl* controller) {
  media::SpeechRecognitionResult result;
  result.transcription = "transcript text 1";
  result.is_final = true;
  result.timing_information = media::TimingInformation();
  result.timing_information->audio_start_time = base::Milliseconds(0);
  result.timing_information->audio_end_time = base::Milliseconds(3000);

  std::vector<media::HypothesisParts> hypothesis_parts;
  std::string hypothesis_text[3] = {"transcript", "text", "1"};
  int hypothesis_time[3] = {1000, 2000, 2500};
  for (int i = 0; i < 3; i++) {
    hypothesis_parts.emplace_back(
        std::vector<std::string>({hypothesis_text[i]}),
        base::Milliseconds(hypothesis_time[i]));
  }

  result.timing_information->hypothesis_parts = std::move(hypothesis_parts);
  controller->OnTranscription(result);
}

void NotifyControllerForPartialSpeechResult(
    ProjectorControllerImpl* controller) {
  controller->OnTranscription(
      media::SpeechRecognitionResult("transcript partial text 1", false));
}

class ProjectorMetadataControllerForTest : public ProjectorMetadataController {
 public:
  ProjectorMetadataControllerForTest() = default;
  ProjectorMetadataControllerForTest(
      const ProjectorMetadataControllerForTest&) = delete;
  ProjectorMetadataControllerForTest& operator=(
      const ProjectorMetadataControllerForTest&) = delete;
  ~ProjectorMetadataControllerForTest() override = default;

  void SetRunLoopQuitClosure(base::RepeatingClosure closure) {
    quit_closure_ = base::BindOnce(closure);
  }

 protected:
  // ProjectorMetadataController:
  void OnSaveFileResult(const base::FilePath& path,
                        size_t transcripts_count,
                        bool success) override {
    ProjectorMetadataController::OnSaveFileResult(path, transcripts_count,
                                                  success);
    std::move(quit_closure_).Run();
  }

 private:
  base::OnceClosure quit_closure_;
};

}  // namespace

class ProjectorControllerTest : public AshTestBase {
 public:
  ProjectorControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ProjectorControllerTest(const ProjectorControllerTest&) = delete;
  ProjectorControllerTest& operator=(const ProjectorControllerTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    controller_ =
        static_cast<ProjectorControllerImpl*>(ProjectorController::Get());

    auto mock_metadata_controller =
        std::make_unique<MockProjectorMetadataController>();
    mock_metadata_controller_ = mock_metadata_controller.get();
    controller_->SetProjectorMetadataControllerForTest(
        std::move(mock_metadata_controller));

    SpeechRecognitionAvailability availability;
    availability.on_device_availability =
        OnDeviceRecognitionAvailability::kAvailable;
    ON_CALL(mock_client_, GetSpeechRecognitionAvailability)
        .WillByDefault(testing::Return(availability));
    controller_->SetClient(&mock_client_);

    auto* annotator_controller = Shell::Get()->annotator_controller();
    annotator_controller->SetToolClient(&mock_annotator_client_);
  }

  void InitializeRealMetadataController() {
    std::unique_ptr<ProjectorMetadataController> metadata_controller =
        std::make_unique<ProjectorMetadataControllerForTest>();
    metadata_controller_ = static_cast<ProjectorMetadataControllerForTest*>(
        metadata_controller.get());
    controller_->SetProjectorMetadataControllerForTest(
        std::move(metadata_controller));
  }

 protected:
  void InitFakeMic(bool mic_present) {
    if (!mic_present) {
      CrasAudioHandler::Get()->SetActiveInputNodes({});
      return;
    }

    const AudioNodeInfo kInternalMic[] = {
        {true, 55555, "Fake Mic", "INTERNAL_MIC", "Internal Mic"}};
    const AudioNode audio_node =
        AudioNode(kInternalMic->is_input, kInternalMic->id,
                  /*has_v2_stable_device_id=*/false, kInternalMic->id,
                  /*stable_device_id_v2=*/0, kInternalMic->device_name,
                  kInternalMic->type, kInternalMic->name, /*active=*/false,
                  /*plugged_time=*/0, /*max_supported_channels=*/1,
                  /*audio_effect=*/1, /*number_of_volume_steps=*/25);
    FakeCrasAudioClient::Get()->SetAudioNodesForTesting({audio_node});

    CrasAudioHandler::Get()->SetActiveInputNodes({kInternalMic->id});
  }

  raw_ptr<MockProjectorMetadataController, DanglingUntriaged>
      mock_metadata_controller_ = nullptr;
  raw_ptr<ProjectorMetadataControllerForTest, DanglingUntriaged>
      metadata_controller_;
  raw_ptr<ProjectorControllerImpl, DanglingUntriaged> controller_;
  MockProjectorClient mock_client_;
  MockAnnotatorClient mock_annotator_client_;
  base::HistogramTester histogram_tester_;
  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ProjectorControllerTest, OnTranscription) {
  // Verify that |RecordTranscription| in |ProjectorMetadataController| is
  // called to record the transcript.
  EXPECT_CALL(*mock_metadata_controller_, RecordTranscription(_)).Times(1);

  NotifyControllerForFinalSpeechResult(controller_);
}

TEST_F(ProjectorControllerTest, OnTranscriptionPartialResult) {
  // Verify that |RecordTranscription| in |ProjectorMetadataController| is not
  // called since it is not a final result.
  EXPECT_CALL(*mock_metadata_controller_, RecordTranscription(_)).Times(0);
  NotifyControllerForPartialSpeechResult(controller_);
}

TEST_F(ProjectorControllerTest, OnAudioNodesChanged) {
  ON_CALL(mock_client_, IsDriveFsMounted())
      .WillByDefault(testing::Return(true));

  InitFakeMic(/*mic_present=*/true);
  EXPECT_CALL(mock_client_,
              OnNewScreencastPreconditionChanged(NewScreencastPrecondition(
                  NewScreencastPreconditionState::kEnabled,
                  {NewScreencastPreconditionReason::kEnabledBySoda})));
  controller_->OnAudioNodesChanged();

  InitFakeMic(/*mic_present=*/false);
  EXPECT_CALL(mock_client_,
              OnNewScreencastPreconditionChanged(NewScreencastPrecondition(
                  NewScreencastPreconditionState::kDisabled,
                  {NewScreencastPreconditionReason::kNoMic})));
  controller_->OnAudioNodesChanged();
}

TEST_F(ProjectorControllerTest, OnSpeechRecognitionAvailabilityChanged) {
  SpeechRecognitionAvailability availability;

  // Soda is not available.
  availability.use_on_device = true;
  availability.on_device_availability =
      OnDeviceRecognitionAvailability::kSodaNotAvailable;
  ON_CALL(mock_client_, GetSpeechRecognitionAvailability)
      .WillByDefault(testing::Return(availability));
  ON_CALL(mock_client_, IsDriveFsMounted())
      .WillByDefault(testing::Return(true));
  EXPECT_CALL(mock_client_,
              OnNewScreencastPreconditionChanged(NewScreencastPrecondition(
                  NewScreencastPreconditionState::kDisabled,
                  {NewScreencastPreconditionReason::
                       kOnDeviceSpeechRecognitionNotSupported})));
  controller_->OnSpeechRecognitionAvailabilityChanged();

  // Soda is available.
  availability.on_device_availability =
      OnDeviceRecognitionAvailability::kAvailable;
  ON_CALL(mock_client_, GetSpeechRecognitionAvailability)
      .WillByDefault(testing::Return(availability));
  EXPECT_CALL(mock_client_,
              OnNewScreencastPreconditionChanged(NewScreencastPrecondition(
                  NewScreencastPreconditionState::kEnabled,
                  {NewScreencastPreconditionReason::kEnabledBySoda})));
  controller_->OnSpeechRecognitionAvailabilityChanged();

  // Server based available.
  availability.use_on_device = false;
  availability.server_based_availability =
      ServerBasedRecognitionAvailability::kAvailable;
  ON_CALL(mock_client_, GetSpeechRecognitionAvailability)
      .WillByDefault(testing::Return(availability));
  EXPECT_CALL(mock_client_,
              OnNewScreencastPreconditionChanged(NewScreencastPrecondition(
                  NewScreencastPreconditionState::kEnabled,
                  {NewScreencastPreconditionReason::
                       kEnabledByServerSideSpeechRecognition})));
  controller_->OnSpeechRecognitionAvailabilityChanged();
}

TEST_F(ProjectorControllerTest, RecordingStarted) {
  EXPECT_CALL(mock_client_, StartSpeechRecognition());
  EXPECT_CALL(*mock_metadata_controller_, OnRecordingStarted());

  auto* root = Shell::GetPrimaryRootWindow();
  controller_->projector_session()->Start(
      base::SafeBaseName::Create("projector_data").value());
  histogram_tester_.ExpectUniqueSample(
      kProjectorCreationFlowHistogramName,
      /*sample=*/ProjectorCreationFlow::kSessionStarted,
      /*expected_bucket_count=*/1);
  controller_->OnRecordingStarted(root);
  histogram_tester_.ExpectBucketCount(
      kProjectorCreationFlowHistogramName,
      /*sample=*/ProjectorCreationFlow::kRecordingStarted,
      /*expected_count=*/1);
}

TEST_F(ProjectorControllerTest, RecordingEnded) {
  base::FilePath screencast_container_path;
  ASSERT_TRUE(mock_client_.GetBaseStoragePath(&screencast_container_path));
  ON_CALL(mock_client_, IsDriveFsMounted())
      .WillByDefault(testing::Return(true));

  EXPECT_CALL(mock_client_, OpenProjectorApp()).Times(0);
  EXPECT_CALL(mock_client_,
              OnNewScreencastPreconditionChanged(NewScreencastPrecondition(
                  NewScreencastPreconditionState::kDisabled,
                  {NewScreencastPreconditionReason::kInProjectorSession})));

  controller_->projector_session()->Start(
      base::SafeBaseName::Create("projector_data").value());
  histogram_tester_.ExpectUniqueSample(
      kProjectorCreationFlowHistogramName,
      /*sample=*/ProjectorCreationFlow::kSessionStarted,
      /*expected_bucket_count=*/1);

  controller_->OnRecordingStarted(Shell::GetPrimaryRootWindow());
  histogram_tester_.ExpectBucketCount(
      kProjectorCreationFlowHistogramName,
      /*sample=*/ProjectorCreationFlow::kRecordingStarted,
      /*expected_count=*/1);

  base::RunLoop runLoop;
  controller_->CreateScreencastContainerFolder(base::BindLambdaForTesting(
      [&](const base::FilePath& screencast_file_path_no_extension) {
        // Expects screencast files name equals to it's parent folder name:
        EXPECT_EQ(screencast_file_path_no_extension.BaseName(),
                  screencast_file_path_no_extension.DirName().BaseName());
        EXPECT_CALL(
            mock_client_,
            OnNewScreencastPreconditionChanged(NewScreencastPrecondition(
                NewScreencastPreconditionState::kEnabled,
                {NewScreencastPreconditionReason::kEnabledBySoda})))
            .Times(0);
        EXPECT_CALL(mock_client_, StopSpeechRecognition())
            .WillOnce(testing::Invoke([&]() {
              controller_->OnSpeechRecognitionStopped(/*forced=*/false);
            }));
        EXPECT_CALL(*mock_metadata_controller_, SaveMetadata(_)).Times(0);

        controller_->OnRecordingEnded();
        runLoop.Quit();
      }));

  runLoop.Run();

  histogram_tester_.ExpectBucketCount(
      kProjectorCreationFlowHistogramName,
      /*sample=*/ProjectorCreationFlow::kRecordingEnded, /*expected_count=*/1);
  histogram_tester_.ExpectTotalCount(kProjectorCreationFlowHistogramName,
                                     /*expected_count=*/3);
}

enum class RecognitionEndLatency {
  // The speech recognition has ended even before recording
  // has wrapped up dlp check.
  kImmediate,
  // The speech recognition ends after recording has wrapped up dlp check
  // but fore the restricted timeout.
  kDelayed,
  // The speech recognition doesn't end and it causes a time out.
  kDelayedCausingTimeout
};

class ProjectorOnDlpRestrictionCheckedAtVideoEndTest
    : public ::testing::WithParamInterface<
          ::testing::tuple<RecognitionEndLatency, bool>>,
      public ProjectorControllerTest {
 public:
  ProjectorOnDlpRestrictionCheckedAtVideoEndTest() = default;
  ProjectorOnDlpRestrictionCheckedAtVideoEndTest(
      const ProjectorOnDlpRestrictionCheckedAtVideoEndTest&) = delete;
  ProjectorOnDlpRestrictionCheckedAtVideoEndTest& operator=(
      const ProjectorOnDlpRestrictionCheckedAtVideoEndTest&) = delete;
  ~ProjectorOnDlpRestrictionCheckedAtVideoEndTest() override = default;
};

TEST_P(ProjectorOnDlpRestrictionCheckedAtVideoEndTest, WrapUpRecordingOnce) {
  bool wrap_up_by_speech_stopped;
  bool transcript_end_timed_out;
  switch (std::get<0>(GetParam())) {
    case RecognitionEndLatency::kImmediate:
      wrap_up_by_speech_stopped = false;
      transcript_end_timed_out = false;
      break;
    case RecognitionEndLatency::kDelayed:
      wrap_up_by_speech_stopped = true;
      transcript_end_timed_out = false;
      break;
    case RecognitionEndLatency::kDelayedCausingTimeout:
      wrap_up_by_speech_stopped = true;
      transcript_end_timed_out = true;
      break;
  }

  bool user_deleted_video_file = std::get<1>(GetParam());
  base::FilePath screencast_container_path;
  ASSERT_TRUE(mock_client_.GetBaseStoragePath(&screencast_container_path));
  ON_CALL(mock_client_, IsDriveFsMounted())
      .WillByDefault(testing::Return(true));

  EXPECT_CALL(mock_client_, OpenProjectorApp());
  EXPECT_CALL(mock_client_,
              OnNewScreencastPreconditionChanged(NewScreencastPrecondition(
                  NewScreencastPreconditionState::kDisabled,
                  {NewScreencastPreconditionReason::kInProjectorSession})));

  // Advance clock to 20:02:10 Jan 2nd, 2021.
  base::Time start_time;
  EXPECT_TRUE(base::Time::FromString("2 Jan 2021 20:02:10", &start_time));
  base::TimeDelta forward_by = start_time - base::Time::Now();
  task_environment()->AdvanceClock(forward_by);

  controller_->projector_session()->Start(
      base::SafeBaseName::Create("projector_data").value());
  histogram_tester_.ExpectUniqueSample(
      kProjectorCreationFlowHistogramName,
      /*sample=*/ProjectorCreationFlow::kSessionStarted,
      /*expected_bucket_count=*/1);

  controller_->OnRecordingStarted(Shell::GetPrimaryRootWindow());
  histogram_tester_.ExpectBucketCount(
      kProjectorCreationFlowHistogramName,
      /*sample=*/ProjectorCreationFlow::kRecordingStarted,
      /*expected_count=*/1);

  base::RunLoop runLoop;
  controller_->CreateScreencastContainerFolder(base::BindLambdaForTesting(
      [&](const base::FilePath& screencast_file_path_no_extension) {
        EXPECT_CALL(
            mock_client_,
            OnNewScreencastPreconditionChanged(NewScreencastPrecondition(
                NewScreencastPreconditionState::kEnabled,
                {NewScreencastPreconditionReason::kEnabledBySoda})));

        const std::string expected_screencast_name =
            "Screencast 2021-01-02 20.02.10";
        const base::FilePath expected_path =
            screencast_container_path.Append("root")
                .Append("projector_data")
                // Screencast container folder.
                .Append(expected_screencast_name)
                // Screencast file name without extension.
                .Append(expected_screencast_name);
        controller_->OnRecordingEnded();
        if (!user_deleted_video_file) {
          // Verify that |SaveMetadata| in |ProjectorMetadataController| is
          // called with the expected path.
          EXPECT_EQ(screencast_file_path_no_extension, expected_path);
          // Verify that save metadata only triggered once. The path will not
          // change as the clock advances.
          task_environment()->AdvanceClock(base::Minutes(1));
          int expected_count = wrap_up_by_speech_stopped ? 2 : 1;
          EXPECT_CALL(*mock_metadata_controller_, SaveMetadata(expected_path))
              .Times(expected_count);
          // Verify that thumbnail file is saved.
          controller_->SetOnFileSavedCallbackForTest(base::BindLambdaForTesting(
              [&](const base::FilePath& path, bool success) {
                EXPECT_TRUE(success);
                EXPECT_TRUE(base::PathExists(path));
              }));
        } else {
          // Verify that save metadata is not triggered.
          EXPECT_CALL(*mock_metadata_controller_, SaveMetadata(_)).Times(0);
          // Expects notification gets resumed if recording deleted.
          const std::vector<base::FilePath> screencast_files = {
              expected_path.AddExtension(kProjectorV2MetadataFileExtension),
              expected_path.AddExtension(kProjectorMediaFileExtension),
              expected_path.DirName().Append(
                  kScreencastDefaultThumbnailFileName)};
          EXPECT_CALL(mock_client_, ToggleFileSyncingNotificationForPaths(
                                        screencast_files, /*suppress=*/false));
          // Verify that Projector Folder is cleaned up.
          controller_->SetOnPathDeletedCallbackForTest(
              base::BindLambdaForTesting(
                  [&](const base::FilePath& path, bool success) {
                    EXPECT_TRUE(success);
                    EXPECT_FALSE(base::PathExists(path));
                  }));
        }

        auto image = gfx::test::CreateImageSkia(10, 10);
        if (wrap_up_by_speech_stopped) {
          controller_->OnVideoFileFinalized(
              /*user_deleted_video_file=*/user_deleted_video_file,
              /*thumbnail=*/image);
          if (!transcript_end_timed_out) {
            controller_->OnSpeechRecognitionStopped(/*forced=*/false);
          } else {
            EXPECT_CALL(mock_client_, ForceEndSpeechRecognition())
                .Times(1)
                .WillOnce(testing::Invoke([&]() {
                  controller_->OnSpeechRecognitionStopped(/*forced=*/true);
                }));

            // Simulate that the timer has fired.
            EXPECT_TRUE(controller_->get_timer_for_testing()->IsRunning());
            controller_->get_timer_for_testing()->FireNow();
          }
        } else {
          controller_->OnSpeechRecognitionStopped(/*forced=*/false);
          controller_->OnVideoFileFinalized(
              /*user_deleted_video_file=*/user_deleted_video_file,
              /*thumbnail=*/image);
        }
        runLoop.Quit();
      }));

  runLoop.Run();

  histogram_tester_.ExpectTotalCount(kProjectorCreationFlowHistogramName,
                                     /*expected_count=*/4);
}

INSTANTIATE_TEST_SUITE_P(
    WrapUpRecordingOnce,
    ProjectorOnDlpRestrictionCheckedAtVideoEndTest,
    ::testing::Combine(
        ::testing::ValuesIn({RecognitionEndLatency::kImmediate,
                             RecognitionEndLatency::kDelayed,
                             RecognitionEndLatency::kDelayedCausingTimeout}),
        ::testing::Bool()));

TEST_F(ProjectorControllerTest, NoTranscriptsTest) {
  InitializeRealMetadataController();
  metadata_controller_->OnRecordingStarted();

  base::RunLoop run_loop;
  metadata_controller_->SetRunLoopQuitClosure(run_loop.QuitClosure());

  // Simulate ending the recording and saving the metadata file.
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath metadata_file(temp_dir_.GetPath().Append(kMetadataFileName));
  metadata_controller_->SaveMetadata(metadata_file);
  run_loop.Run();

  histogram_tester_.ExpectUniqueSample(kProjectorTranscriptsCountHistogramName,
                                       /*sample=*/0, /*count=*/1);

  // Verify the written metadata file size is between 0-100 bytes. Change this
  // limit as needed if you make significant changes to the metadata file.
  base::File file(metadata_file.AddExtension(kProjectorV2Extension),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  EXPECT_GT(file.GetLength(), 0);
  EXPECT_LT(file.GetLength(), 100);
}

TEST_F(ProjectorControllerTest, TranscriptsTest) {
  InitializeRealMetadataController();
  metadata_controller_->OnRecordingStarted();

  base::RunLoop run_loop;
  metadata_controller_->SetRunLoopQuitClosure(run_loop.QuitClosure());

  // Simulate adding some transcripts.
  NotifyControllerForFinalSpeechResult(controller_);
  NotifyControllerForFinalSpeechResult(controller_);

  // Simulate ending the recording and saving the metadata file.
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath metadata_file(temp_dir_.GetPath().Append(kMetadataFileName));
  metadata_controller_->SaveMetadata(metadata_file);
  run_loop.Run();

  histogram_tester_.ExpectUniqueSample(kProjectorTranscriptsCountHistogramName,
                                       /*sample=*/2, /*count=*/1);

  // Verify the written metadata file size is between 400-500 bytes. This file
  // should be larger than the one in the NoTranscriptsTest above. Change this
  // limit as needed if you make significant changes to the metadata file.
  base::File file(metadata_file.AddExtension(kProjectorV2Extension),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  EXPECT_GT(file.GetLength(), 400);
  EXPECT_LT(file.GetLength(), 500);
}

TEST_F(ProjectorControllerTest, V2TranscriptsTest) {
  InitializeRealMetadataController();
  metadata_controller_->OnRecordingStarted();

  base::RunLoop run_loop;
  metadata_controller_->SetRunLoopQuitClosure(run_loop.QuitClosure());

  // Simulate adding some transcripts.
  NotifyControllerForFinalSpeechResult(controller_);
  NotifyControllerForFinalSpeechResult(controller_);

  // Simulate ending the recording and saving the metadata file.
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath metadata_file(temp_dir_.GetPath().Append(kMetadataFileName));
  metadata_controller_->SaveMetadata(metadata_file);
  run_loop.Run();

  histogram_tester_.ExpectUniqueSample(kProjectorTranscriptsCountHistogramName,
                                       /*sample=*/2, /*count=*/1);

  // Verify the written metadata file size is between 400-500 bytes. This file
  // should be larger than the one in the NoTranscriptsTest above. Change this
  // limit as needed if you make significant changes to the metadata file.
  base::File file(metadata_file.AddExtension(kProjectorV2Extension),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  EXPECT_GT(file.GetLength(), 400);
  EXPECT_LT(file.GetLength(), 500);
}

TEST_F(ProjectorControllerTest, OnDriveMountFailed) {
  ON_CALL(mock_client_, IsDriveFsMountFailed())
      .WillByDefault(testing::Return(true));
  ON_CALL(mock_client_, IsDriveFsMounted())
      .WillByDefault(testing::Return(false));

  EXPECT_EQ(NewScreencastPrecondition(
                NewScreencastPreconditionState::kDisabled,
                {NewScreencastPreconditionReason::kDriveFsMountFailed}),
            controller_->GetNewScreencastPrecondition());
}

TEST_F(ProjectorControllerTest, SuppressDriveNotification) {
  ON_CALL(mock_client_, IsDriveFsMounted())
      .WillByDefault(testing::Return(true));

  base::FilePath mounted_path;
  ASSERT_TRUE(mock_client_.GetBaseStoragePath(&mounted_path));

  // The screencast name, which is used to form the screencast folder/files
  // paths, is generated on projector session starts
  auto* projector_session = controller_->projector_session();
  projector_session->Start(
      base::SafeBaseName::Create("projector_data").value());
  const base::FilePath expect_container_path =
      mounted_path.Append("root")
          .Append(projector_session->storage_dir())
          .Append(projector_session->screencast_name());

  const base::FilePath expected_path_with_no_extension =
      expect_container_path.Append(projector_session->screencast_name());

  const std::vector<base::FilePath> screencast_files = {
      expected_path_with_no_extension.AddExtension(
          kProjectorV2MetadataFileExtension),
      expected_path_with_no_extension.AddExtension(
          kProjectorMediaFileExtension),
      expect_container_path.Append(kScreencastDefaultThumbnailFileName)};

  // Expects notification gets suppressed when creating screencast folder.
  EXPECT_CALL(mock_client_, ToggleFileSyncingNotificationForPaths(
                                screencast_files, /*suppress=*/true))
      .Times(1);
  base::RunLoop run_loop;
  controller_->CreateScreencastContainerFolder(base::BindLambdaForTesting(
      [&](const base::FilePath& screencast_file_path_no_extension) {
        EXPECT_EQ(expected_path_with_no_extension,
                  screencast_file_path_no_extension);
        // Expects notification gets resumed if recording is aborted.
        EXPECT_CALL(mock_client_, ToggleFileSyncingNotificationForPaths(
                                      screencast_files, /*suppress=*/false))
            .Times(1);
        // Simulates starting abort called by capture mode.
        controller_->OnRecordingStartAborted();
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Used to test SpeechRecognitionEndState metric for both on-device and
// server based speech recognition.
class ProjectorSpeechRecognitionEndTest
    : public ::testing::WithParamInterface<bool>,
      public ProjectorControllerTest {
 public:
  ProjectorSpeechRecognitionEndTest() = default;
  ProjectorSpeechRecognitionEndTest(const ProjectorSpeechRecognitionEndTest&) =
      delete;
  ProjectorSpeechRecognitionEndTest& operator=(
      const ProjectorSpeechRecognitionEndTest&) = delete;
  ~ProjectorSpeechRecognitionEndTest() override = default;
};

TEST_P(ProjectorSpeechRecognitionEndTest, SpeechRecognitionEndMetric) {
  SpeechRecognitionAvailability availability;
  availability.on_device_availability =
      OnDeviceRecognitionAvailability::kAvailable;
  availability.server_based_availability =
      ServerBasedRecognitionAvailability::kAvailable;
  availability.use_on_device = GetParam();

  ON_CALL(mock_client_, GetSpeechRecognitionAvailability)
      .WillByDefault(testing::Return(availability));
  const std::string histogram_name =
      availability.use_on_device ? kSpeechRecognitionEndStateOnDevice
                                 : kSpeechRecognitionEndStateServerBased;
  auto* projector_session = controller_->projector_session();
  projector_session->Start(
      base::SafeBaseName::Create("projector_data").value());

  auto* root = Shell::GetPrimaryRootWindow();

  // Tests speech recognition encountering an error during session.
  controller_->OnRecordingStarted(root);
  controller_->OnTranscriptionError();
  histogram_tester_.ExpectBucketCount(
      histogram_name,
      SpeechRecognitionEndState::kSpeechRecognitionEnounteredError,
      /*expected_count=*/1);

  // Tests speech recognition successfully stopping.
  ON_CALL(mock_client_, StopSpeechRecognition)
      .WillByDefault(testing::Invoke([&]() {
        controller_->OnSpeechRecognitionStopped(/*forced=*/false);
      }));
  controller_->OnRecordingStarted(root);
  controller_->OnRecordingEnded();
  histogram_tester_.ExpectBucketCount(
      histogram_name,
      SpeechRecognitionEndState::kSpeechRecognitionSuccessfullyStopped,
      /*expected_count=*/1);

  // Tests speech recognition forced stopped.
  ON_CALL(mock_client_, StopSpeechRecognition).WillByDefault(testing::Return());
  EXPECT_CALL(mock_client_, ForceEndSpeechRecognition())
      .Times(1)
      .WillOnce(testing::Invoke(
          [&]() { controller_->OnSpeechRecognitionStopped(/*forced=*/true); }));
  controller_->OnRecordingStarted(root);
  controller_->OnRecordingEnded();
  controller_->get_timer_for_testing()->FireNow();
  histogram_tester_.ExpectBucketCount(
      histogram_name,
      SpeechRecognitionEndState::kSpeechRecognitionForcedStopped,
      /*expected_count=*/1);

  // Tests speech recognition encountering error while stopping.
  controller_->OnRecordingStarted(root);
  controller_->OnRecordingEnded();
  controller_->OnTranscriptionError();
  histogram_tester_.ExpectBucketCount(
      histogram_name,
      SpeechRecognitionEndState::
          kSpeechRecognitionEncounteredErrorWhileStopping,
      /*expected_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(SpeechRecognitionEndMetric,
                         ProjectorSpeechRecognitionEndTest,
                         /*use_on_device=*/::testing::Bool());

}  // namespace ash
