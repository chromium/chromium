// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_GRADUATION_GRADUATION_STATE_TRACKER_H_
#define ASH_WEBUI_GRADUATION_GRADUATION_STATE_TRACKER_H_

namespace ash::graduation {

// Tracks the app state as the user progresses through the Content Transfer
// flow. Records the final state on app close before this tracker is destroyed.
class GraduationStateTracker {
 public:
  // The flow state used to record the status of the flow when the app window is
  // closed. Should be kept consistent with the ContentTransferFlowState enum in
  // tools/metrics/histograms/metadata/ash/enums.xml.
  enum class FlowState : int {
    // The welcome screen has been displayed to the user.
    kWelcome = 0,
    // The user has navigated to the Takeout Transfer screen.
    kTakeoutUi = 1,
    // The WebUI has indicated that the user completed the Transfer.
    kTakeoutTransferComplete = 2,
    // The error screen has been shown to the user.
    kError = 3,
    kNumStates = 4,
  };

  static constexpr char kFlowStateHistogramName[] =
      "Ash.ContentTransfer.FlowState";

  GraduationStateTracker();
  GraduationStateTracker(const GraduationStateTracker&) = delete;
  GraduationStateTracker& operator=(const GraduationStateTracker&) = delete;
  ~GraduationStateTracker();

  void set_flow_state(FlowState state) { flow_state_ = state; }

 private:
  FlowState flow_state_ = FlowState::kWelcome;
};
}  // namespace ash::graduation

#endif  // ASH_WEBUI_GRADUATION_GRADUATION_STATE_TRACKER_H_
