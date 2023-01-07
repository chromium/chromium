// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_KEY_WINDOW_NOTIFIER_H_
#define CHROME_BROWSER_MAC_KEY_WINDOW_NOTIFIER_H_

#include "base/observer_list.h"

// This class manages observers that listen to events about key windows on
// macOS.
// https://developer.apple.com/documentation/appkit/nsapplication/1428406-keywindow
class KeyWindowNotifier {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnNoKeyWindow() = 0;
  };

  KeyWindowNotifier();
  ~KeyWindowNotifier();

  KeyWindowNotifier(const KeyWindowNotifier&) = delete;
  KeyWindowNotifier& operator=(KeyWindowNotifier&&) = delete;

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  // This notification is sent when the app has no key window, such as when
  // all windows are closed but the app is still active.
  void NotifyNoKeyWindow();

 private:
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_MAC_KEY_WINDOW_NOTIFIER_H_
