// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the input method candidate window used on Chrome OS.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_H_


namespace chromeos {
namespace input_method {

// CandidateWindowController is used for controlling the input method
// candidate window. Once the initialization is done, the controller
// starts monitoring signals sent from the the background input method
// daemon, and shows and hides the candidate window as neeeded. Upon
// deletion of the object, monitoring stops and the view used for
// rendering the candidate view is deleted.
class CandidateWindowController {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void CandidateClicked(int index) = 0;
    virtual void CandidateWindowOpened() = 0;
    virtual void CandidateWindowClosed() = 0;
  };

  virtual ~CandidateWindowController() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual void Hide() = 0;

  // Gets an instance of CandidateWindowController. Caller has to delete the
  // returned object.
  static CandidateWindowController* CreateCandidateWindowController();
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_CANDIDATE_WINDOW_CONTROLLER_H_
