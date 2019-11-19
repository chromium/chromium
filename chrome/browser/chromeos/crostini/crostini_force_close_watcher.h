// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_FORCE_CLOSE_WATCHER_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_FORCE_CLOSE_WATCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "ui/views/widget/widget_observer.h"

namespace exo {
class ShellSurfaceBase;
}

namespace crostini {

// Class that observes user attempts to close crostini windows, and notifies its
// delegate if it thinks the window needs to be force-closed.
class ForceCloseWatcher : public views::WidgetObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    // Callback to get the widget this watcher should be watching.
    virtual views::Widget* GetClosableWidget() = 0;

    // Called by the |watcher| when it has finished initializing.
    virtual void Watched(ForceCloseWatcher* watcher) = 0;

    // Called when the watcher decides to begin the force-close operation.
    virtual void Prompt() = 0;

    // Called when the watcher wants to abort an in-progress force-close
    // operation, e.g. because the window we're trying to close did disappear
    // after Prompt() was called.
    virtual void Hide() = 0;
  };

  // Creates a watcher for the given |delegate| which will wait for attempts to
  // close its associated widget and, if needed, bring up a UI allowing the user
  // to forcibly close its associated window.
  //
  // The watcher works as follows:
  //
  //                                          Cancel
  //                                     +------------+
  //                                     |            |
  //             X             5 sec     V    X       |
  // "Watching"---->"Waiting"-------->"Active"---->"Dialog"
  //                    |                |          |   |
  //      window closes +----------------+----------+   | Force Close
  //          itself    v                           |   V
  //                  "Done"<--------------------"Force closing"
  //                             Destroy shell
  //                                surface
  //
  // Importantly, we require *two* attempts to close the window before bringing
  // up the dialog, which is needed as windows may legitimately refuse to close
  // (e.g. if you have unsaved work open).
  static void Watch(std::unique_ptr<Delegate> delegate);

  // WidgetObserver overrides.
  void OnWidgetDestroying(views::Widget* widget) override;

  // Called each time a user tries to close the widget being observed.
  void OnCloseRequested();

  // Sets the timeout to the given value, for use in tests.
  void OverrideDelayForTesting(base::TimeDelta delay);

 private:
  // Create a ForceCloseWatcher to monitor close requests on the given
  // |delegate|'s associated widget.
  explicit ForceCloseWatcher(std::unique_ptr<Delegate> delegate);

  ~ForceCloseWatcher() override;

  std::unique_ptr<Delegate> delegate_;

  // Time delay required before bringing up the UI.
  base::TimeDelta force_close_delay_;

  // Implements the delay between the first and second time the user tries to
  // close the window.
  base::Optional<base::ElapsedTimer> show_dialog_timer_;

  DISALLOW_COPY_AND_ASSIGN(ForceCloseWatcher);
};

// The delegate implementation to allow exo's shell surfaces to be closed by the
// watcher.
class ShellSurfaceForceCloseDelegate : public ForceCloseWatcher::Delegate,
                                       public views::WidgetObserver {
 public:
  ShellSurfaceForceCloseDelegate(exo::ShellSurfaceBase* shell_surface,
                                 std::string app_name);

  ~ShellSurfaceForceCloseDelegate() override;

  void ForceClose();

  // ForceCloseWatcher::Delegate overrides.
  views::Widget* GetClosableWidget() override;
  void Watched(ForceCloseWatcher* watcher) override;
  void Prompt() override;
  void Hide() override;

  // WidgetObserver overrides.
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  // Handle to the shell surface we are trying to close.
  exo::ShellSurfaceBase* shell_surface_;

  // Name of the app we are trying to close (or "" if unknown).
  std::string app_name_;

  // Handle to the widget representing the currently visible force-close dialog
  // (if there is one), or null.
  views::Widget* current_dialog_ = nullptr;

  base::WeakPtrFactory<ShellSurfaceForceCloseDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ShellSurfaceForceCloseDelegate);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_FORCE_CLOSE_WATCHER_H_
