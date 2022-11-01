// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_IDLE_BROWSER_CLOSER_H_
#define CHROME_BROWSER_ENTERPRISE_IDLE_BROWSER_CLOSER_H_

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/idle_dialog.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace enterprise_idle {

// The "close_browsers" action is different from other actions.
//
// - It shows a 30s dialog before closing, which allows the user to abort the
//   close.
//
// - It runs *before* other actions, so ActionsRunner needs to wait for this
//   flow to finish (or abort).
//
// - If multiple Profiles ask to close at the same time, we want to run other
//   actions after they're *all* done closing (which is asynchronous).
//
// A centralized BrowserCloser singleton receives close requests, and calls the
// observers when it's done.
class BrowserCloser {
 public:
  enum class CloseResult {
    // The dialog expired, and then the browsers closed successfully.
    kSuccess,
    // One of these 2 scenarios:
    // - The dialog was dismissed by the user, so we didn't close the browsers.
    // - We tried to close browsers, but failed for some reason.
    kAborted,
    // No browsers to close, so nothing to do. Dialog was not shown.
    kSkip,
  };

  static BrowserCloser* GetInstance();

  // Shows the 30s dialog, then closes all browsers with `profile` or one of its
  // OTR profiles (e.g. Incognito).
  base::CallbackListSubscription ShowDialogAndCloseBrowsers(
      Profile* profile,
      base::TimeDelta threshold,
      base::OnceCallback<void(CloseResult)> on_finished);

  void DismissDialogForTesting();

 private:
  friend class base::NoDestructor<BrowserCloser>;

  BrowserCloser();
  ~BrowserCloser();

  // Runs after 30s without the user dismissing the dialog.
  void OnDialogExpired();

  // Runs when the user hits Escape, or clicks the "Continue using Chrome"
  // button in the dialog.
  void OnDialogDismissedByUser();

  // Callbacks for BrowserList::CloseAllBrowsersWithProfile().
  void OnCloseSuccess(const base::FilePath& profile_dir);
  void OnCloseAborted(const base::FilePath& profile_dir);

  // Set of profiles that are currently closing. Stored as FilePaths instead of
  // Profile*, so we don't have to worry about dangling profile pointers.
  base::flat_set<base::FilePath> closing_profiles_;

  // Pending `on_finished` callbacks.
  base::OnceCallbackList<void(CloseResult)> callbacks_;

  base::WeakPtr<views::Widget> dialog_;

  // Timer for `dialog_`. Runs OnDialogExpired().
  base::OneShotTimer dialog_timer_;
};

}  // namespace enterprise_idle

#endif  // CHROME_BROWSER_ENTERPRISE_IDLE_BROWSER_CLOSER_H_
