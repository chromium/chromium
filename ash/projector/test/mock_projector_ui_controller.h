// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_TEST_MOCK_PROJECTOR_UI_CONTROLLER_H_
#define ASH_PROJECTOR_TEST_MOCK_PROJECTOR_UI_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/projector/projector_ui_controller.h"
#include "ash/public/cpp/projector/annotator_tool.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class ProjectorControllerImpl;

// A mock implementation of ProjectorUiController for use in tests.
class ASH_EXPORT MockProjectorUiController : public ProjectorUiController {
 public:
  explicit MockProjectorUiController(
      ProjectorControllerImpl* projector_controller);

  MockProjectorUiController(const MockProjectorUiController&) = delete;
  MockProjectorUiController& operator=(const MockProjectorUiController&) =
      delete;

  ~MockProjectorUiController() override;

  // ProjectorUiController:
  MOCK_METHOD1(ShowAnnotationTray, void(aura::Window*));
  MOCK_METHOD0(HideAnnotationTray, void());
  MOCK_METHOD0(EnableAnnotatorTool, void());
  MOCK_METHOD1(SetAnnotatorTool, void(const AnnotatorTool&));
};

}  // namespace ash
#endif  // ASH_PROJECTOR_TEST_MOCK_PROJECTOR_UI_CONTROLLER_H_
