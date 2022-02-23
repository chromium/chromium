// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_MOCK_PROJECTOR_CONTROLLER_H_
#define ASH_PUBLIC_CPP_TEST_MOCK_PROJECTOR_CONTROLLER_H_

#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockProjectorController : public ash::ProjectorController {
 public:
  MockProjectorController();
  MockProjectorController(const MockProjectorController&) = delete;
  MockProjectorController& operator=(const MockProjectorController&) = delete;
  ~MockProjectorController() override;

  // ProjectorController:
  MOCK_METHOD1(StartProjectorSession, void(const std::string& storageDir));
  MOCK_METHOD1(SetClient, void(ash::ProjectorClient* client));
  MOCK_METHOD1(OnSpeechRecognitionAvailabilityChanged,
               void(SpeechRecognitionAvailability availability));
  MOCK_METHOD1(OnTranscription,
               void(const media::SpeechRecognitionResult& result));
  MOCK_METHOD0(OnTranscriptionError, void());
  MOCK_METHOD0(OnSpeechRecognitionStopped, void());
  MOCK_METHOD1(SetProjectorToolsVisible, void(bool is_visible));
  MOCK_CONST_METHOD0(IsEligible, bool());
  MOCK_CONST_METHOD0(GetNewScreencastPrecondition, NewScreencastPrecondition());
  MOCK_METHOD1(OnToolSet, void(const AnnotatorTool& tool));
  MOCK_METHOD2(OnUndoRedoAvailabilityChanged,
               void(bool undo_available, bool redo_available));
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_MOCK_PROJECTOR_CONTROLLER_H_
