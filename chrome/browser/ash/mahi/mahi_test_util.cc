// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_test_util.h"

#include <memory>
#include <set>

#include "ash/system/mahi/mahi_ui_update.h"
#include "ash/system/mahi/test/mock_mahi_ui_controller_delegate.h"
#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/mahi/mahi_manager_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// Aliases ---------------------------------------------------------------------

using ::testing::NiceMock;

// Constants -------------------------------------------------------------------

constexpr char kFakeSummary[] = "fake summary";
constexpr char kFakeAnswer[] = "fake answer";

// UiUpdateRecorder ------------------------------------------------------------

// Records the types of the Mahi UI updates received during its life cycle.
class UiUpdateRecorder {
 public:
  explicit UiUpdateRecorder(MahiUiController* controller) {
    mock_controller_delegate_ =
        std::make_unique<NiceMock<MockMahiUiControllerDelegate>>(controller);
    ON_CALL(*mock_controller_delegate_, OnUpdated)
        .WillByDefault([this](const MahiUiUpdate& update) {
          received_updates_.insert(update.type());
        });
  }

  bool HasUpdate(MahiUiUpdateType type) const {
    return base::Contains(received_updates_, type);
  }

 private:
  std::unique_ptr<NiceMock<MockMahiUiControllerDelegate>>
      mock_controller_delegate_;

  std::set<MahiUiUpdateType> received_updates_;
};

// Helpers ---------------------------------------------------------------------

MahiManagerImpl* GetMahiManager() {
  return static_cast<MahiManagerImpl*>(chromeos::MahiManager::Get());
}

}  // namespace

const char* GetMahiDefaultTestAnswer() {
  return kFakeAnswer;
}

const char* GetMahiDefaultTestSummary() {
  return kFakeSummary;
}

MahiUiController* GetMahiUiController() {
  return GetMahiManager()->ui_controller_for_test();
}

// Waits until the specified `MahiUiUpdate` is received.
void WaitUntilUiUpdateReceived(MahiUiUpdateType target_type) {
  NiceMock<MockMahiUiControllerDelegate> mock_controller_delegate(
      GetMahiUiController());

  base::RunLoop run_loop;
  ON_CALL(mock_controller_delegate, OnUpdated)
      .WillByDefault([&run_loop, target_type](const MahiUiUpdate& update) {
        if (update.type() == target_type) {
          run_loop.Quit();
        }
      });
  run_loop.Run();
}

}  // namespace ash
