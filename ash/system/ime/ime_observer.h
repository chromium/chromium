// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_IME_IME_OBSERVER_H_
#define ASH_SYSTEM_IME_IME_OBSERVER_H_

namespace ash {

class IMEObserver {
 public:
  virtual ~IMEObserver() {}

  // Notify the observer that the IME state has changed, and should be
  // refreshed.
  virtual void OnIMERefresh() = 0;

  // Notify the observer that the IME menu activation state has changed, and
  // should be refreshed. |is_active| represents whether the new IME menu is
  // active, and IME related items in system tray should be removed if
  // |is_active| is true.
  virtual void OnIMEMenuActivationChanged(bool is_active) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_IME_IME_OBSERVER_H_
