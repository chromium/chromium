// Copyright 2021 The Chromium Authors
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
  MOCK_METHOD1(StartProjectorSession,
               void(const base::SafeBaseName& storageDir));
  MOCK_METHOD1(SetClient, void(ash::ProjectorClient* client));
  MOCK_METHOD0(OnSpeechRecognitionAvailabilityChanged, void());
  MOCK_METHOD1(OnTranscription,
               void(const media::SpeechRecognitionResult& result));
  MOCK_METHOD0(OnTranscriptionError, void());
  MOCK_METHOD1(OnSpeechRecognitionStopped, void(bool forced));
  MOCK_METHOD1(SetProjectorToolsVisible, void(bool is_visible));
  MOCK_CONST_METHOD0(GetNewScreencastPrecondition, NewScreencastPrecondition());
  MOCK_METHOD2(OnUndoRedoAvailabilityChanged,
               void(bool undo_available, bool redo_available));
  MOCK_METHOD1(OnCanvasInitialized, void(bool success));
  MOCK_METHOD0(GetAnnotatorAvailability, bool());
  MOCK_METHOD0(ToggleAnnotationTray, void());
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_MOCK_PROJECTOR_CONTROLLER_H_
