// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_MOCK_CANDIDATE_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_MOCK_CANDIDATE_WINDOW_CONTROLLER_H_

#include "base/observer_list.h"
#include "chrome/browser/ash/input_method/candidate_window_controller.h"

namespace ash {
namespace input_method {

// The mock CandidateWindowController implementation for testing.
class MockCandidateWindowController : public CandidateWindowController {
 public:
  MockCandidateWindowController();

  MockCandidateWindowController(const MockCandidateWindowController&) = delete;
  MockCandidateWindowController& operator=(
      const MockCandidateWindowController&) = delete;

  ~MockCandidateWindowController() override;

  // CandidateWindowController overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void Hide() override;

  // Notifies observers.
  void NotifyCandidateWindowOpened();
  void NotifyCandidateWindowClosed();

  int add_observer_count_;
  int remove_observer_count_;
  int hide_count_;

 private:
  base::ObserverList<CandidateWindowController::Observer>::Unchecked observers_;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_MOCK_CANDIDATE_WINDOW_CONTROLLER_H_
