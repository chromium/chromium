// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_MOCK_PROJECTOR_CLIENT_H_
#define ASH_PUBLIC_CPP_TEST_MOCK_PROJECTOR_CLIENT_H_

#include "ash/public/cpp/projector/projector_client.h"
#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/public/cpp/projector/speech_recognition_availability.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class FilePath;
}

namespace ash {

// A mock implementation of ProjectorClient for use in tests.
class MockProjectorClient : public ProjectorClient {
 public:
  MockProjectorClient();
  MockProjectorClient(const MockProjectorClient&) = delete;
  MockProjectorClient& operator=(const MockProjectorClient&) = delete;
  ~MockProjectorClient() override;

  // ProjectorClient:
  MOCK_CONST_METHOD0(GetSpeechRecognitionAvailability,
                     SpeechRecognitionAvailability());
  MOCK_METHOD0(StartSpeechRecognition, void());
  MOCK_METHOD0(StopSpeechRecognition, void());
  MOCK_METHOD0(ForceEndSpeechRecognition, void());
  bool GetBaseStoragePath(base::FilePath* result) const override;
  MOCK_CONST_METHOD0(IsDriveFsMounted, bool());
  MOCK_CONST_METHOD0(IsDriveFsMountFailed, bool());
  MOCK_CONST_METHOD0(OpenProjectorApp, void());
  MOCK_CONST_METHOD0(MinimizeProjectorApp, void());
  MOCK_CONST_METHOD0(CloseProjectorApp, void());
  MOCK_CONST_METHOD1(OnNewScreencastPreconditionChanged,
                     void(const NewScreencastPrecondition&));
  MOCK_METHOD2(ToggleFileSyncingNotificationForPaths,
               void(const std::vector<base::FilePath>&, bool));

 private:
  base::ScopedTempDir screencast_container_path_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_MOCK_PROJECTOR_CLIENT_H_
