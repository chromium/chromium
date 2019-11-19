// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/mock_candidate_window_controller.h"

namespace chromeos {
namespace input_method {

MockCandidateWindowController::MockCandidateWindowController()
    : add_observer_count_(0),
      remove_observer_count_(0),
      hide_count_(0) {
}

MockCandidateWindowController::~MockCandidateWindowController() = default;

void MockCandidateWindowController::AddObserver(
    CandidateWindowController::Observer* observer) {
  ++add_observer_count_;
  observers_.AddObserver(observer);
}

void MockCandidateWindowController::RemoveObserver(
    CandidateWindowController::Observer* observer) {
  ++remove_observer_count_;
  observers_.RemoveObserver(observer);
}

void MockCandidateWindowController::Hide() {
  ++hide_count_;
}

void MockCandidateWindowController::NotifyCandidateWindowOpened() {
  for (auto& observer : observers_)
    observer.CandidateWindowOpened();
}

void MockCandidateWindowController::NotifyCandidateWindowClosed() {
  for (auto& observer : observers_)
    observer.CandidateWindowClosed();
}

}  // namespace input_method
}  // namespace chromeos
