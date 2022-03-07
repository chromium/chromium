// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_TEST_MOCK_PROJECTOR_CLIENT_H_
#define ASH_PROJECTOR_TEST_MOCK_PROJECTOR_CLIENT_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/projector/annotator_tool.h"
#include "ash/public/cpp/projector/projector_annotator_controller.h"
#include "ash/public/cpp/projector/projector_client.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class FilePath;
}

namespace ash {

// A mock implementation of ProjectorClient for use in tests.
class ASH_EXPORT MockProjectorClient : public ProjectorClient,
                                       public ProjectorAnnotatorController {
 public:
  MockProjectorClient();
  MockProjectorClient(const MockProjectorClient&) = delete;
  MockProjectorClient& operator=(const MockProjectorClient&) = delete;
  ~MockProjectorClient() override;

  // ProjectorClient:
  MOCK_METHOD0(StartSpeechRecognition, void());
  MOCK_METHOD0(StopSpeechRecognition, void());
  bool GetDriveFsMountPointPath(base::FilePath* result) const override;
  MOCK_CONST_METHOD0(IsDriveFsMounted, bool());
  MOCK_CONST_METHOD0(IsDriveFsMountFailed, bool());
  MOCK_METHOD0(ShowSelfieCam, void());
  MOCK_METHOD0(CloseSelfieCam, void());
  MOCK_CONST_METHOD0(OpenProjectorApp, void());
  MOCK_CONST_METHOD0(MinimizeProjectorApp, void());
  MOCK_CONST_METHOD1(OnNewScreencastPreconditionChanged,
                     void(const NewScreencastPrecondition&));
  MOCK_METHOD1(SetAnnotatorMessageHandler, void(AnnotatorMessageHandler*));

  // ProjectorAnnotatorController:
  MOCK_METHOD1(SetTool, void(const AnnotatorTool&));
  MOCK_METHOD0(Undo, void());
  MOCK_METHOD0(Redo, void());
  MOCK_METHOD0(Clear, void());

  bool IsSelfieCamVisible() const override;
  void SetSelfieCamVisible(bool visible);

 private:
  base::ScopedTempDir screencast_container_path_;
  bool is_selfie_cam_visible_ = false;
};

}  // namespace ash

#endif  // ASH_PROJECTOR_TEST_MOCK_PROJECTOR_CLIENT_H_
