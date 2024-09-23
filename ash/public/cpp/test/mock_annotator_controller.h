// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_MOCK_ANNOTATOR_CONTROLLER_H_
#define ASH_PUBLIC_CPP_TEST_MOCK_ANNOTATOR_CONTROLLER_H_

#include "ash/public/cpp/annotator/annotator_controller_base.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockAnnotatorController : public ash::AnnotatorControllerBase {
 public:
  MockAnnotatorController();
  MockAnnotatorController(const MockAnnotatorController&) = delete;
  MockAnnotatorController& operator=(const MockAnnotatorController&) = delete;
  ~MockAnnotatorController() override;

  // AnnotatorControllerBase:
  MOCK_METHOD1(SetToolClient, void(AnnotatorClient* client));
  MOCK_CONST_METHOD0(GetAnnotatorAvailability, bool());
  MOCK_METHOD1(OnCanvasInitialized, void(bool success));
  MOCK_METHOD0(ToggleAnnotationTray, void());
  MOCK_METHOD2(OnUndoRedoAvailabilityChanged,
               void(bool undo_available, bool redo_available));
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_MOCK_ANNOTATOR_CONTROLLER_H_
