// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMAND_OBSERVER_H_
#define CHROME_BROWSER_COMMAND_OBSERVER_H_

// An Observer interface implemented by objects that want to be informed when
// the state of a particular command ID is modified. See CommandUpdater.
class CommandObserver {
 public:
  // Notifies the observer that the enabled state has changed for the
  // specified command id.
  virtual void EnabledStateChangedForCommand(int id, bool enabled) = 0;

 protected:
  virtual ~CommandObserver() {}
};

#endif  // CHROME_BROWSER_COMMAND_OBSERVER_H_
