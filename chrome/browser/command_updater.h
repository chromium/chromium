// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMAND_UPDATER_H_
#define CHROME_BROWSER_COMMAND_UPDATER_H_

#include "base/time/time.h"
#include "ui/base/window_open_disposition.h"

class CommandObserver;

////////////////////////////////////////////////////////////////////////////////
//
// CommandUpdater interface
//
//   This is the public API to manage the enabled state of a set of commands.
//   Observers register to listen to changes in this state so they can update
//   their presentation.
//
//   The actual implementation of this is in CommandUpdaterImpl, this interface
//   exists purely so that classes using the actual CommandUpdaterImpl can
//   expose it through a safe public interface (as opposed to directly exposing
//   the private implementation details).
//
class CommandUpdater {
 public:
  virtual ~CommandUpdater() {}

  // Returns true if the specified command ID is supported.
  virtual bool SupportsCommand(int id) const = 0;

  // Returns true if the specified command ID is enabled. The command ID must be
  // supported by this updater.
  virtual bool IsCommandEnabled(int id) const = 0;

  // Performs the action associated with this command ID using CURRENT_TAB
  // disposition.
  // Returns true if the command was executed (i.e. it is supported and is
  // enabled).
  virtual bool ExecuteCommand(
      int id,
      base::TimeTicks time_stamp = base::TimeTicks::Now()) = 0;

  // Performs the action associated with this command ID using the given
  // disposition.
  // Returns true if the command was executed (i.e. it is supported and is
  // enabled).
  virtual bool ExecuteCommandWithDisposition(
      int id,
      WindowOpenDisposition disposition,
      base::TimeTicks time_stamp = base::TimeTicks::Now()) = 0;

  // Adds an observer to the state of a particular command. If the command does
  // not exist, it is created, initialized to false.
  virtual void AddCommandObserver(int id, CommandObserver* observer) = 0;

  // Removes an observer to the state of a particular command.
  virtual void RemoveCommandObserver(int id, CommandObserver* observer) = 0;

  // Removes |observer| for all commands on which it's registered.
  virtual void RemoveCommandObserver(CommandObserver* observer) = 0;

  // Notify all observers of a particular command that the command has been
  // enabled or disabled. If the command does not exist, it is created and
  // initialized to |state|. This function is very lightweight if the command
  // state has not changed.
  // Returns true if the update succeeded (it's possible that the browser is in
  // "locked-down" state where we prevent changes to the command state).
  virtual bool UpdateCommandEnabled(int id, bool state) = 0;
};

#endif  // CHROME_BROWSER_COMMAND_UPDATER_H_
