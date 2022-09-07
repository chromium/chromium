// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_SCREEN_SHARE_OBSERVER_H_
#define ASH_SYSTEM_PRIVACY_SCREEN_SHARE_OBSERVER_H_

#include <string>

#include "base/callback.h"

namespace ash {

class ScreenShareObserver {
 public:
  // Called when screen share is started.
  // |stop_callback| must be a base::RepeatingCallback so that it can be passed
  // to all observers.
  virtual void OnScreenShareStart(const base::RepeatingClosure& stop_callback,
                                  const std::u16string& helper_name) = 0;

  // Called when screen share is stopped.
  virtual void OnScreenShareStop() = 0;

 protected:
  virtual ~ScreenShareObserver() {}
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_SCREEN_SHARE_OBSERVER_H_
