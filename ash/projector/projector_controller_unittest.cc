// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_controller_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/projector/test/mock_projector_metadata_controller.h"
#include "ash/projector/test/mock_projector_ui_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/services/machine_learning/public/mojom/soda.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using chromeos::machine_learning::mojom::FinalResult;
using chromeos::machine_learning::mojom::PartialResult;
using chromeos::machine_learning::mojom::SpeechRecognizerEvent;
using chromeos::machine_learning::mojom::SpeechRecognizerEventPtr;
using chromeos::machine_learning::mojom::TimingInfo;
using testing::_;
using testing::ElementsAre;

void NotifyControllerForFinalSpeechResult(ProjectorControllerImpl* controller) {
  controller->OnTranscription(
      base::UTF8ToUTF16("transcript text 1"),
      base::TimeDelta::FromMilliseconds(0) /* audio_start_time */,
      base::TimeDelta::FromMilliseconds(3000) /* audio_end_time */,
      {{base::TimeDelta::FromMilliseconds(1000),
        base::TimeDelta::FromMilliseconds(2000),
        base::TimeDelta::FromMilliseconds(2500)}} /* word_offsets*/,
      true /* is_final */);
}

void NotifyControllerForPartialSpeechResult(
    ProjectorControllerImpl* controller) {
  controller->OnTranscription(
      base::UTF8ToUTF16("transcript partial text 1"),
      base::TimeDelta::FromMilliseconds(0) /* audio_start_time */,
      base::TimeDelta::FromMilliseconds(3000) /* audio_end_time */,
      {{base::TimeDelta::FromMilliseconds(1000),
        base::TimeDelta::FromMilliseconds(2000),
        base::TimeDelta::FromMilliseconds(2500),
        base::TimeDelta::FromMilliseconds(3000)}} /* word_offsets*/,
      false /* is_final */);
}

}  // namespace

class ProjectorControllerTest : public AshTestBase {
 public:
  ProjectorControllerTest() = default;

  ProjectorControllerTest(const ProjectorControllerTest&) = delete;
  ProjectorControllerTest& operator=(const ProjectorControllerTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    controller_ = std::make_unique<ProjectorControllerImpl>();

    auto mock_ui_controller =
        std::make_unique<MockProjectorUiController>(nullptr);
    mock_ui_controller_ = mock_ui_controller.get();
    controller_->SetProjectorUiControllerForTest(std::move(mock_ui_controller));

    auto mock_metadata_controller =
        std::make_unique<MockProjectorMetadataController>();
    mock_metadata_controller_ = mock_metadata_controller.get();
    controller_->SetProjectorMetadataControllerForTest(
        std::move(mock_metadata_controller));
  }

 protected:
  MockProjectorUiController* mock_ui_controller_ = nullptr;
  MockProjectorMetadataController* mock_metadata_controller_ = nullptr;
  std::unique_ptr<ProjectorControllerImpl> controller_;
};

TEST_F(ProjectorControllerTest, ShowToolbar) {
  // Verify that |ShowToolbar| in |ProjectorUiController| is called.
  EXPECT_CALL(*mock_ui_controller_, ShowToolbar()).Times(1);
  controller_->ShowToolbar();
}

TEST_F(ProjectorControllerTest, SaveScreencast) {
  base::FilePath saved_path;
  // Verify that |SaveMetadata| in |ProjectorMetadataController| is called.
  EXPECT_CALL(*mock_metadata_controller_, SaveMetadata(saved_path)).Times(1);
  controller_->SaveScreencast(saved_path);
}

TEST_F(ProjectorControllerTest, OnTranscription) {
  // Verify that |RecordTranscription| in |ProjectorMetadataController| is
  // called to record the transcript.
  EXPECT_CALL(
      *mock_metadata_controller_,
      RecordTranscription("transcript text 1",
                          testing::Eq(base::TimeDelta::FromMilliseconds(0)),
                          testing::Eq(base::TimeDelta::FromMilliseconds(3000)),
                          ElementsAre(base::TimeDelta::FromMilliseconds(1000),
                                      base::TimeDelta::FromMilliseconds(2000),
                                      base::TimeDelta::FromMilliseconds(2500))))
      .Times(1);
  // Verify that |OnTranscription| in |ProjectorUiController| is not called
  // since capton is off.
  EXPECT_CALL(*mock_ui_controller_, OnTranscription(_, _)).Times(0);
  NotifyControllerForFinalSpeechResult(controller_.get());
}

TEST_F(ProjectorControllerTest, OnTranscriptionPartialResult) {
  // Verify that |RecordTranscription| in |ProjectorMetadataController| is not
  // called since it is not a final result.
  EXPECT_CALL(*mock_metadata_controller_, RecordTranscription(_, _, _, _))
      .Times(0);
  // Verify that |OnTranscription| in |ProjectorUiController| is not called
  // since caption is off.
  EXPECT_CALL(*mock_ui_controller_, OnTranscription(_, _)).Times(0);
  NotifyControllerForPartialSpeechResult(controller_.get());
}

TEST_F(ProjectorControllerTest, OnTranscriptionCaptionOn) {
  // Verify that |SaveMetadata| in |ProjectorMetadataController| is called to
  // record the transcript.
  EXPECT_CALL(
      *mock_metadata_controller_,
      RecordTranscription("transcript text 1",
                          testing::Eq(base::TimeDelta::FromMilliseconds(0)),
                          testing::Eq(base::TimeDelta::FromMilliseconds(3000)),
                          ElementsAre(base::TimeDelta::FromMilliseconds(1000),
                                      base::TimeDelta::FromMilliseconds(2000),
                                      base::TimeDelta::FromMilliseconds(2500))))
      .Times(1);
  // Verify that |OnTranscription| in |ProjectorUiController| is called since
  // capton is on.
  EXPECT_CALL(*mock_ui_controller_, OnTranscription("transcript text 1", true))
      .Times(1);
  controller_->SetCaptionState(true);
  NotifyControllerForFinalSpeechResult(controller_.get());
}

TEST_F(ProjectorControllerTest, OnTranscriptionCaptionOnPartialResult) {
  // Verify that |RecordTranscription| in |ProjectorMetadataController| is
  // called.
  EXPECT_CALL(*mock_metadata_controller_, RecordTranscription(_, _, _, _))
      .Times(0);
  // Verify that |OnTranscription| in |ProjectorUiController| is called since
  // capton is on.
  EXPECT_CALL(*mock_ui_controller_,
              OnTranscription("transcript partial text 1", false))
      .Times(1);
  controller_->SetCaptionState(true);
  NotifyControllerForPartialSpeechResult(controller_.get());
}

TEST_F(ProjectorControllerTest, OnSpeechRecognitionAvailable) {
  controller_->OnSpeechRecognitionAvailable(true);
  EXPECT_TRUE(controller_->is_eligible());

  controller_->OnSpeechRecognitionAvailable(false);
  EXPECT_FALSE(controller_->is_eligible());
}

}  // namespace ash
