// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMAND_UPDATER_IMPL_H_
#define CHROME_BROWSER_COMMAND_UPDATER_IMPL_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/command_updater.h"
#include "ui/base/window_open_disposition.h"

class CommandObserver;
class CommandUpdaterDelegate;

////////////////////////////////////////////////////////////////////////////////
//
// CommandUpdaterImpl class
//
//   This object manages the enabled state of a set of commands. Observers
//   register to listen to changes in this state so they can update their
//   presentation.
//
class CommandUpdaterImpl : public CommandUpdater {
 public:
  // Create a CommandUpdaterImpl with |delegate| to handle the execution of
  // specific commands.
  explicit CommandUpdaterImpl(CommandUpdaterDelegate* delegate);

  CommandUpdaterImpl(const CommandUpdaterImpl&) = delete;
  CommandUpdaterImpl& operator=(const CommandUpdaterImpl&) = delete;

  ~CommandUpdaterImpl() override;

  // Overriden from CommandUpdater:
  bool SupportsCommand(int id) const override;
  bool IsCommandEnabled(int id) const override;
  bool ExecuteCommand(
      int id,
      base::TimeTicks time_stamp = base::TimeTicks::Now()) override;
  bool ExecuteCommandWithDisposition(
      int id,
      WindowOpenDisposition disposition,
      base::TimeTicks time_stamp = base::TimeTicks::Now()) override;
  void AddCommandObserver(int id, CommandObserver* observer) override;
  void RemoveCommandObserver(int id, CommandObserver* observer) override;
  void RemoveCommandObserver(CommandObserver* observer) override;
  bool UpdateCommandEnabled(int id, bool state) override;

  void DisableAllCommands();
  std::vector<int> GetAllIds();

 private:
  // A piece of data about a command - whether or not it is enabled, and a list
  // of objects that observe the enabled state of this command.
  struct Command;

  // Get a Command node for a given command ID, creating an entry if it doesn't
  // exist if desired.
  Command* GetCommand(int id, bool create);

  // The delegate is responsible for executing commands.
  raw_ptr<CommandUpdaterDelegate> delegate_;

  // This is a map of command IDs to states and observer lists
  std::unordered_map<int, std::unique_ptr<Command>> commands_;
};

#endif  // CHROME_BROWSER_COMMAND_UPDATER_IMPL_H_
