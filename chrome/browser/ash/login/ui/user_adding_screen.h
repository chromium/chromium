// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_USER_ADDING_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_USER_ADDING_SCREEN_H_

#include "base/macros.h"

namespace chromeos {

// An interface that defines screen for adding users into multi-profile session.
// Current implementation is a singleton.
// TODO(dzhioev): get rid of singleton.
class UserAddingScreen {
 public:
  struct Observer {
    virtual void OnBeforeUserAddingScreenStarted() {}
    virtual void OnUserAddingFinished() {}
    virtual ~Observer() {}
  };

  static UserAddingScreen* Get();

  virtual void Start() = 0;
  virtual void Cancel() = 0;
  virtual bool IsRunning() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  UserAddingScreen();
  virtual ~UserAddingScreen();

  DISALLOW_COPY_AND_ASSIGN(UserAddingScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_USER_ADDING_SCREEN_H_
