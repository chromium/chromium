// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_TEST_MOCK_PROJECTOR_CLIENT_H_
#define ASH_PROJECTOR_TEST_MOCK_PROJECTOR_CLIENT_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/projector/projector_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

// A mock implementation of ProjectorClient for use in tests.
class ASH_EXPORT MockProjectorClient : public ProjectorClient {
 public:
  MockProjectorClient();
  MockProjectorClient(const MockProjectorClient&) = delete;
  MockProjectorClient& operator=(const MockProjectorClient&) = delete;
  virtual ~MockProjectorClient();

  // ProjectorClient:
  MOCK_METHOD0(StartSpeechRecognition, void());
  MOCK_METHOD0(StopSpeechRecognition, void());
  MOCK_METHOD0(ShowSelfieCam, void());
  MOCK_METHOD0(CloseSelfieCam, void());

  bool IsSelfieCamVisible() const override;
  void SetSelfieCamVisible(bool visible);

 private:
  bool is_selfie_cam_visible_ = false;
};

}  // namespace ash

#endif  // ASH_PROJECTOR_TEST_MOCK_PROJECTOR_CLIENT_H_
