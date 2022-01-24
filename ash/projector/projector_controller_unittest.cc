// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_controller_impl.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/projector/model/projector_session_impl.h"
#include "ash/projector/test/mock_projector_client.h"
#include "ash/projector/test/mock_projector_metadata_controller.h"
#include "ash/projector/test/mock_projector_ui_controller.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {
namespace {
using testing::_;
using testing::ElementsAre;

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

}  // namespace

class ProjectorControllerTest : public AshTestBase {
 public:
  ProjectorControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(features::kProjector);
  }

  ProjectorControllerTest(const ProjectorControllerTest&) = delete;
  ProjectorControllerTest& operator=(const ProjectorControllerTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    controller_ =
        static_cast<ProjectorControllerImpl*>(ProjectorController::Get());

    auto mock_ui_controller =
        std::make_unique<MockProjectorUiController>(controller_);
    mock_ui_controller_ = mock_ui_controller.get();
    controller_->SetProjectorUiControllerForTest(std::move(mock_ui_controller));

    auto mock_metadata_controller =
        std::make_unique<MockProjectorMetadataController>();
    mock_metadata_controller_ = mock_metadata_controller.get();
    controller_->SetProjectorMetadataControllerForTest(
        std::move(mock_metadata_controller));

    controller_->SetClient(&mock_client_);
    controller_->OnSpeechRecognitionAvailable(/*available=*/true);
  }

 protected:
  MockProjectorUiController* mock_ui_controller_ = nullptr;
  MockProjectorMetadataController* mock_metadata_controller_ = nullptr;
  ProjectorControllerImpl* controller_;
  MockProjectorClient mock_client_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ProjectorControllerTest, OnTranscription) {
  // Verify that |RecordTranscription| in |ProjectorMetadataController| is
  // called to record the transcript.
  EXPECT_CALL(*mock_metadata_controller_, RecordTranscription(_)).Times(1);
  // Verify that |OnTranscription| in |ProjectorUiController| is not called
  // since capton is off.
  EXPECT_CALL(*mock_ui_controller_, OnTranscription(_, _)).Times(0);
  NotifyControllerForFinalSpeechResult(controller_);
}

TEST_F(ProjectorControllerTest, OnTranscriptionPartialResult) {
  // Verify that |RecordTranscription| in |ProjectorMetadataController| is not
  // called since it is not a final result.
  EXPECT_CALL(*mock_metadata_controller_, RecordTranscription(_)).Times(0);
  // Verify that |OnTranscription| in |ProjectorUiController| is not called
  // since caption is off.
  EXPECT_CALL(*mock_ui_controller_, OnTranscription(_, _)).Times(0);
  NotifyControllerForPartialSpeechResult(controller_);
}

TEST_F(ProjectorControllerTest, OnTranscriptionCaptionOn) {
  // Verify that |SaveMetadata| in |ProjectorMetadataController| is called to
  // record the transcript.
  EXPECT_CALL(*mock_metadata_controller_, RecordTranscription(_)).Times(1);
  // Verify that |OnTranscription| in |ProjectorUiController| is called since
  // capton is on.
  EXPECT_CALL(*mock_ui_controller_, OnTranscription("transcript text 1", true))
      .Times(1);
  controller_->OnCaptionBubbleModelStateChanged(true);
  NotifyControllerForFinalSpeechResult(controller_);
}

TEST_F(ProjectorControllerTest, OnTranscriptionCaptionOnPartialResult) {
  // Verify that |RecordTranscription| in |ProjectorMetadataController| is not
  // called since it is not a final result.
  EXPECT_CALL(*mock_metadata_controller_, RecordTranscription(_)).Times(0);
  // Verify that |OnTranscription| in |ProjectorUiController| is called since
  // capton is on.
  EXPECT_CALL(*mock_ui_controller_,
              OnTranscription("transcript partial text 1", false))
      .Times(1);
  controller_->OnCaptionBubbleModelStateChanged(true);
  NotifyControllerForPartialSpeechResult(controller_);
}

TEST_F(ProjectorControllerTest, OnSpeechRecognitionAvailable) {
  controller_->OnSpeechRecognitionAvailable(true);
  EXPECT_TRUE(controller_->IsEligible());

  controller_->OnSpeechRecognitionAvailable(false);
  EXPECT_FALSE(controller_->IsEligible());
}

TEST_F(ProjectorControllerTest, OnLaserPointerPressed) {
  // Verify that |OnLaserPointerPressed| in |ProjectorUiController| is called.
  EXPECT_CALL(*mock_ui_controller_, OnLaserPointerPressed());
  controller_->OnLaserPointerPressed();
}

TEST_F(ProjectorControllerTest, OnMarkerPressed) {
  // Verify that |OnMarkerPressed| in |ProjectorUiController| is called.
  EXPECT_CALL(*mock_ui_controller_, OnMarkerPressed());
  controller_->OnMarkerPressed();
}

TEST_F(ProjectorControllerTest, OnClearAllMarkersPressed) {
  // Verify that |OnClearAllMarkersPressed| in |ProjectorUiController| is
  // called.
  EXPECT_CALL(*mock_ui_controller_, OnClearAllMarkersPressed());
  controller_->OnClearAllMarkersPressed();
}

TEST_F(ProjectorControllerTest, OnSelfieCamPressed) {
  // Verify that |OnSelfieCamPressed| in |ProjectorUiController| is called.
  EXPECT_CALL(*mock_ui_controller_, OnSelfieCamPressed(/*enabled=*/true));
  EXPECT_CALL(mock_client_, ShowSelfieCam());
  controller_->OnSelfieCamPressed(/*enabled=*/true);
  mock_client_.SetSelfieCamVisible(/*visible=*/true);

  EXPECT_CALL(*mock_ui_controller_, OnSelfieCamPressed(/*enabled=*/false));
  EXPECT_CALL(mock_client_, CloseSelfieCam());
  controller_->OnSelfieCamPressed(/*enabled=*/false);
  mock_client_.SetSelfieCamVisible(/*visible=*/false);
}

TEST_F(ProjectorControllerTest, SetCaptionBubbleState) {
  EXPECT_CALL(*mock_ui_controller_, SetCaptionBubbleState(true));
  controller_->SetCaptionBubbleState(true);
}

TEST_F(ProjectorControllerTest, MagnifierButtonPressed) {
  EXPECT_CALL(*mock_ui_controller_, OnMagnifierButtonPressed(true));
  controller_->OnMagnifierButtonPressed(true);
}

TEST_F(ProjectorControllerTest, OnChangeMarkerColorPressed) {
  EXPECT_CALL(*mock_ui_controller_, OnChangeMarkerColorPressed(SK_ColorBLACK));
  controller_->OnChangeMarkerColorPressed(SK_ColorBLACK);
}

TEST_F(ProjectorControllerTest, OnUndoPressed) {
  EXPECT_CALL(*mock_ui_controller_, OnUndoPressed());
  controller_->OnUndoPressed();
}

TEST_F(ProjectorControllerTest, RecordingStarted) {
  EXPECT_CALL(mock_client_, StartSpeechRecognition());
  EXPECT_CALL(*mock_ui_controller_, OnRecordingStateChanged(/*started=*/true));
  EXPECT_CALL(*mock_metadata_controller_, OnRecordingStarted());
  mock_client_.SetSelfieCamVisible(/*visible=*/true);
  // Verify that |CloseToolbar| in |ProjectorUiController| is called.
  EXPECT_CALL(*mock_ui_controller_, ShowToolbar()).Times(1);

  controller_->OnRecordingStarted();
}

TEST_F(ProjectorControllerTest, RecordingEnded) {
  base::FilePath screencast_container_path;
  ASSERT_TRUE(
      mock_client_.GetDriveFsMountPointPath(&screencast_container_path));
  ON_CALL(mock_client_, IsDriveFsMounted())
      .WillByDefault(testing::Return(true));

  mock_client_.SetSelfieCamVisible(/*visible=*/true);
  // Verify that |CloseToolbar| in |ProjectorUiController| is called.
  EXPECT_CALL(*mock_ui_controller_, CloseToolbar()).Times(1);
  EXPECT_CALL(mock_client_, CloseSelfieCam()).Times(1);
  EXPECT_CALL(mock_client_, OpenProjectorApp());
  EXPECT_CALL(mock_client_,
              OnNewScreencastPreconditionChanged(/*can_start=*/false));

  // Advance clock to 20:02:10 Jan 2nd, 2021.
  base::Time start_time;
  EXPECT_TRUE(base::Time::FromString("2 Jan 2021 20:02:10", &start_time));
  base::TimeDelta forward_by = start_time - base::Time::Now();
  task_environment()->AdvanceClock(forward_by);
  controller_->projector_session()->Start("projector_data");
  controller_->OnRecordingStarted();

  base::RunLoop runLoop;
  controller_->CreateScreencastContainerFolder(base::BindLambdaForTesting(
      [&](const base::FilePath& screencast_file_path_no_extension) {
        EXPECT_CALL(mock_client_,
                    OnNewScreencastPreconditionChanged(/*can_start=*/true));

        EXPECT_CALL(mock_client_, StopSpeechRecognition());
        EXPECT_CALL(*mock_ui_controller_,
                    OnRecordingStateChanged(/*started=*/false));

        // Verify that |SaveMetadata| in |ProjectorMetadataController| is called
        // with the expected path.
        const std::string expected_screencast_name =
            "Screencast 2021-01-02 20.02.10";
        EXPECT_CALL(*mock_metadata_controller_,
                    SaveMetadata(screencast_container_path.Append("root")
                                     .Append("projector_data")
                                     // Screencast container folder.
                                     .Append(expected_screencast_name)
                                     // Screencast file name without extension.
                                     .Append(expected_screencast_name)));

        controller_->OnRecordingEnded();
        runLoop.Quit();
      }));

  runLoop.Run();
}
}  // namespace ash
