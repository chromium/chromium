// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/screen_ai/screen_ai_install_state.h"

#include "base/files/file_path.h"
#include "base/scoped_observation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestScreenAIInstallState : public screen_ai::ScreenAIInstallState {
 public:
  TestScreenAIInstallState() = default;

  TestScreenAIInstallState(const TestScreenAIInstallState&) = delete;
  TestScreenAIInstallState& operator=(const TestScreenAIInstallState&) = delete;

  ~TestScreenAIInstallState() override = default;

  void SetLastUsageTime() override {}

  void DownloadComponentInternal() override {
    // The passed file path is not used and just indicates that the component
    // exists.
    ScreenAIInstallState::GetInstance()->SetComponentFolder(
        base::FilePath(FILE_PATH_LITERAL("tmp")));
  }
};

}  // namespace

namespace screen_ai {

class ScreenAIInstallStateTest : public testing::Test,
                                 ScreenAIInstallState::Observer {
 public:
  ScreenAIInstallStateTest() {
    ScreenAIInstallState::SetDeviceCompatibleForTesting(true);
    test_install_state_ = std::make_unique<TestScreenAIInstallState>();
  }

  void StartObservation() {
    component_downloaded_observer_.Observe(ScreenAIInstallState::GetInstance());
  }

  void SimulateSuccessfulDownload() {
    test_install_state_->DownloadComponentInternal();
  }

  void StateChanged(ScreenAIInstallState::State state) override {
    if (state == ScreenAIInstallState::State::kDownloaded) {
      component_downloaded_received_ = true;
    }
  }

  void OnScreenAIInstallStateDestroying() override {
    component_downloaded_observer_.Reset();
  }

  bool ComponentDownloadedReceived() { return component_downloaded_received_; }

  bool IsObserving() const {
    return component_downloaded_observer_.IsObserving();
  }

  void DestroyInstallState() { test_install_state_.reset(); }

 private:
  std::unique_ptr<TestScreenAIInstallState> test_install_state_;

  base::ScopedObservation<screen_ai::ScreenAIInstallState,
                          ScreenAIInstallState::Observer>
      component_downloaded_observer_{this};

  bool component_downloaded_received_ = false;
};

TEST_F(ScreenAIInstallStateTest, AddingObserverTriggersDownload) {
  StartObservation();
  EXPECT_TRUE(ComponentDownloadedReceived());
}

TEST_F(ScreenAIInstallStateTest, DownloadedBeforeObservation) {
  SimulateSuccessfulDownload();
  StartObservation();
  EXPECT_TRUE(ComponentDownloadedReceived());
}

TEST_F(ScreenAIInstallStateTest, ObservationAfterFailure) {
  ScreenAIInstallState::GetInstance()->SetStateForTesting(
      screen_ai::ScreenAIInstallState::State::kDownloadFailed);
  StartObservation();
  EXPECT_TRUE(ComponentDownloadedReceived());
}

TEST_F(ScreenAIInstallStateTest, DestroyStateBeforeObserver) {
  StartObservation();
  EXPECT_TRUE(IsObserving());
  DestroyInstallState();
  EXPECT_FALSE(IsObserving());
}

TEST_F(ScreenAIInstallStateTest, ResetForTestingNotifiesObservers) {
  StartObservation();
  EXPECT_TRUE(IsObserving());
  ScreenAIInstallState::GetInstance()->ResetForTesting();
  EXPECT_FALSE(IsObserving());
}

TEST_F(ScreenAIInstallStateTest, RestoreDefaultInstanceAfterOverride) {
  // Destroy the test instance created by the fixture so that GetInstance()
  // resolves to the default process singleton.
  DestroyInstallState();
  ScreenAIInstallState* default_instance = ScreenAIInstallState::GetInstance();
  ASSERT_NE(default_instance, nullptr);
  EXPECT_EQ(ScreenAIInstallState::GetInstance(), default_instance);

  {
    TestScreenAIInstallState scoped_install_state_1;
    EXPECT_EQ(ScreenAIInstallState::GetInstance(), &scoped_install_state_1);

    {
      TestScreenAIInstallState scoped_install_state_2;
      EXPECT_EQ(ScreenAIInstallState::GetInstance(), &scoped_install_state_2);
    }

    EXPECT_EQ(ScreenAIInstallState::GetInstance(), &scoped_install_state_1);
  }

  EXPECT_EQ(ScreenAIInstallState::GetInstance(), default_instance);
}

TEST_F(ScreenAIInstallStateTest, DeviceCompatibility) {
  ScreenAIInstallState::SetDeviceCompatibleForTesting(true);
  EXPECT_TRUE(ScreenAIInstallState::IsDeviceCompatible());

  ScreenAIInstallState::SetDeviceCompatibleForTesting(false);
  EXPECT_FALSE(ScreenAIInstallState::IsDeviceCompatible());
}

TEST_F(ScreenAIInstallStateTest, VerifyLibraryVersion) {
  ScreenAIInstallState::SetDeviceCompatibleForTesting(true);
  EXPECT_TRUE(
      ScreenAIInstallState::VerifyLibraryVersion(base::Version("140.3")));
  EXPECT_TRUE(
      ScreenAIInstallState::VerifyLibraryVersion(base::Version("141.0")));
  EXPECT_FALSE(
      ScreenAIInstallState::VerifyLibraryVersion(base::Version("140.2")));
  EXPECT_FALSE(ScreenAIInstallState::VerifyLibraryVersion(base::Version("")));

  ScreenAIInstallState::SetDeviceCompatibleForTesting(false);
  EXPECT_FALSE(
      ScreenAIInstallState::VerifyLibraryVersion(base::Version("140.3")));
  EXPECT_FALSE(
      ScreenAIInstallState::VerifyLibraryVersion(base::Version("141.0")));
}

TEST_F(ScreenAIInstallStateTest, IncompatibleDeviceDownloadFails) {
  ScreenAIInstallState::SetDeviceCompatibleForTesting(false);
  StartObservation();
  EXPECT_FALSE(ComponentDownloadedReceived());
  EXPECT_EQ(ScreenAIInstallState::GetInstance()->get_state(),
            ScreenAIInstallState::State::kDownloadFailed);
  EXPECT_FALSE(ScreenAIInstallState::GetInstance()->IsComponentAvailable());
}

TEST_F(ScreenAIInstallStateTest, IncompatibleDeviceTriggerDownloadFails) {
  ScreenAIInstallState::SetDeviceCompatibleForTesting(false);
  ScreenAIInstallState::GetInstance()->DownloadComponent();
  EXPECT_EQ(ScreenAIInstallState::GetInstance()->get_state(),
            ScreenAIInstallState::State::kDownloadFailed);
  EXPECT_FALSE(ScreenAIInstallState::GetInstance()->IsComponentAvailable());
}

TEST_F(ScreenAIInstallStateTest,
       DownloadedComponentNotOverwrittenOnIncompatibleDevice) {
  // An incompatible device in production would never reach the `kDownloaded`
  // state because ShouldInstall() prevents installation at startup,
  // VerifyLibraryVersion() rejects the library binary, and DownloadComponent()
  // returns early on demand. However, tests frequently mock an installed
  // component via SetComponentFolder(); calling DownloadComponent() or
  // AddObserver() must not overwrite that state.
  SimulateSuccessfulDownload();
  EXPECT_EQ(ScreenAIInstallState::GetInstance()->get_state(),
            ScreenAIInstallState::State::kDownloaded);
  EXPECT_TRUE(ScreenAIInstallState::GetInstance()->IsComponentAvailable());

  ScreenAIInstallState::SetDeviceCompatibleForTesting(false);

  // Calling DownloadComponent() directly must not overwrite the downloaded
  // state.
  ScreenAIInstallState::GetInstance()->DownloadComponent();
  EXPECT_EQ(ScreenAIInstallState::GetInstance()->get_state(),
            ScreenAIInstallState::State::kDownloaded);

  // Adding an observer triggers DownloadComponent(), which must also not
  // overwrite the downloaded state.
  StartObservation();
  EXPECT_TRUE(ComponentDownloadedReceived());
  EXPECT_EQ(ScreenAIInstallState::GetInstance()->get_state(),
            ScreenAIInstallState::State::kDownloaded);
  EXPECT_TRUE(ScreenAIInstallState::GetInstance()->IsComponentAvailable());
}

}  // namespace screen_ai
