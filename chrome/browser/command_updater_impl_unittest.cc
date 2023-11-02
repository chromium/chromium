// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/command_updater_impl.h"

#include "base/compiler_specific.h"
#include "chrome/browser/command_observer.h"
#include "chrome/browser/command_updater_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

class FakeCommandUpdaterDelegate : public CommandUpdaterDelegate {
 public:
  void ExecuteCommandWithDisposition(int id, WindowOpenDisposition) override {
    EXPECT_EQ(1, id);
  }
};

class FakeCommandObserver : public CommandObserver {
 public:
  FakeCommandObserver() : enabled_(true) {}

  void EnabledStateChangedForCommand(int id, bool enabled) override {
    enabled_ = enabled;
  }

  bool enabled() const { return enabled_; }

 private:
  bool enabled_;
};

TEST(CommandUpdaterImplTest, TestBasicAPI) {
  FakeCommandUpdaterDelegate delegate;
  CommandUpdaterImpl command_updater(&delegate);

  // Unsupported command
  EXPECT_FALSE(command_updater.SupportsCommand(0));
  EXPECT_FALSE(command_updater.IsCommandEnabled(0));
  // FakeCommandUpdaterDelegate::ExecuteCommand should not be called, since
  // the command is not supported.
  command_updater.ExecuteCommand(0);

  // Supported, enabled command
  command_updater.UpdateCommandEnabled(1, true);
  EXPECT_TRUE(command_updater.SupportsCommand(1));
  EXPECT_TRUE(command_updater.IsCommandEnabled(1));
  command_updater.ExecuteCommand(1);

  // Supported, disabled command
  command_updater.UpdateCommandEnabled(2, false);
  EXPECT_TRUE(command_updater.SupportsCommand(2));
  EXPECT_FALSE(command_updater.IsCommandEnabled(2));
  // FakeCommandUpdaterDelegate::ExecuteCommmand should not be called, since
  // the command_updater is disabled
  command_updater.ExecuteCommand(2);
}

TEST(CommandUpdaterImplTest, TestObservers) {
  FakeCommandUpdaterDelegate delegate;
  CommandUpdaterImpl command_updater(&delegate);

  // Create an observer for the command 2 and add it to the controller, then
  // update the command.
  FakeCommandObserver observer;
  command_updater.AddCommandObserver(2, &observer);
  command_updater.UpdateCommandEnabled(2, true);
  EXPECT_TRUE(observer.enabled());
  command_updater.UpdateCommandEnabled(2, false);
  EXPECT_FALSE(observer.enabled());

  // Remove the observer and update the command.
  command_updater.RemoveCommandObserver(2, &observer);
  command_updater.UpdateCommandEnabled(2, true);
  EXPECT_FALSE(observer.enabled());
}

TEST(CommandUpdaterImplTest, TestObserverRemovingAllCommands) {
  FakeCommandUpdaterDelegate delegate;
  CommandUpdaterImpl command_updater(&delegate);

  // Create two observers for the commands 1-3 as true, remove one using the
  // single remove command, then set the command to false. Ensure that the
  // removed observer still thinks all commands are true and the one left
  // observing picked up the change.

  FakeCommandObserver observer_remove, observer_keep;
  command_updater.AddCommandObserver(1, &observer_remove);
  command_updater.AddCommandObserver(2, &observer_remove);
  command_updater.AddCommandObserver(3, &observer_remove);
  command_updater.AddCommandObserver(1, &observer_keep);
  command_updater.AddCommandObserver(2, &observer_keep);
  command_updater.AddCommandObserver(3, &observer_keep);
  command_updater.UpdateCommandEnabled(1, true);
  command_updater.UpdateCommandEnabled(2, true);
  command_updater.UpdateCommandEnabled(3, true);
  EXPECT_TRUE(observer_remove.enabled());

  // Remove one observer and update the command. Check the states, which
  // should be different.
  command_updater.RemoveCommandObserver(&observer_remove);
  command_updater.UpdateCommandEnabled(1, false);
  command_updater.UpdateCommandEnabled(2, false);
  command_updater.UpdateCommandEnabled(3, false);
  EXPECT_TRUE(observer_remove.enabled());
  EXPECT_FALSE(observer_keep.enabled());
}
