// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/browser_closer.h"

#include <iterator>
#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"

namespace enterprise_idle {

namespace {

constexpr base::TimeDelta kDialogTimeout = base::Seconds(30);

bool ProfileHasBrowsers(const Profile* profile) {
  DCHECK(profile);
  profile = profile->GetOriginalProfile();
  return base::ranges::any_of(
      *BrowserList::GetInstance(), [profile](Browser* browser) {
        return browser->profile()->GetOriginalProfile() == profile;
      });
}

}  // namespace

// static
BrowserCloser* BrowserCloser::GetInstance() {
  static base::NoDestructor<BrowserCloser> instance;
  return instance.get();
}

BrowserCloser::BrowserCloser() = default;

BrowserCloser::~BrowserCloser() = default;

base::CallbackListSubscription BrowserCloser::ShowDialogAndCloseBrowsers(
    Profile* profile,
    base::TimeDelta threshold,
    base::OnceCallback<void(CloseResult)> on_finished) {
  if (!ProfileHasBrowsers(profile)) {
    // No browsers to close for this profile. No need to show a dialog or close
    // browsers, so finish immediately.
    std::move(on_finished).Run(CloseResult::kSkip);
    return base::CallbackListSubscription();
  }

  // Passed the guards: we're really going to show the dialog and close
  // browsers.
  closing_profiles_.insert(profile->GetPath());
  base::CallbackListSubscription subscription =
      callbacks_.Add(std::move(on_finished));

  if (dialog_) {
    // The dialog is already visible, re-use it.
    return subscription;
  }

  dialog_ = IdleDialog::Show(
      kDialogTimeout, threshold,
      base::BindRepeating(&BrowserCloser::OnDialogDismissedByUser,
                          base::Unretained(this)));
  dialog_timer_.Start(
      FROM_HERE, kDialogTimeout,
      base::BindOnce(&BrowserCloser::OnDialogExpired, base::Unretained(this)));
  return subscription;
}

void BrowserCloser::DismissDialogForTesting() {
  CHECK_IS_TEST();
  OnDialogDismissedByUser();
}

void BrowserCloser::OnDialogExpired() {
  DCHECK(!closing_profiles_.empty());

  if (dialog_)
    dialog_->Close();
  dialog_.reset();
  dialog_timer_.Stop();

  // If we did CloseAllbrowsersWithProfile() right away, OnCloseSuccess() might
  // run immediately, in which case we would try to modify `closing_profiles_`
  // while iterating on it.
  //
  // Collect the profiles in `profiles_to_close`, and iterate on *that* instead.
  base::flat_set<Profile*> profiles_to_close;
  base::ranges::transform(
      closing_profiles_,
      std::inserter(profiles_to_close, profiles_to_close.end()),
      [](const base::FilePath& profile_dir) {
        return g_browser_process->profile_manager()->GetProfileByPath(
            profile_dir);
      });

  for (Profile* profile : profiles_to_close) {
    if (!ProfileHasBrowsers(profile)) {
      // Can't close a profile with no browsers. The browsers may have been
      // closed programmatically (e.g. by an extension) during the 30s delay.
      closing_profiles_.erase(profile->GetPath());
      continue;
    }
    // TODO(crbug.com/1316551): Get customer feedback on whether
    // skip_beforeunload should be true or false.
    BrowserList::CloseAllBrowsersWithProfile(
        profile,
        base::BindRepeating(&BrowserCloser::OnCloseSuccess,
                            base::Unretained(this)),
        base::BindRepeating(&BrowserCloser::OnCloseAborted,
                            base::Unretained(this)),
        /*skip_beforeunload=*/true);
  }

  // We showed the dialog, but then no profiles needed closing. Count this as a
  // "success".
  if (closing_profiles_.empty())
    OnCloseSuccess(base::FilePath());
}

void BrowserCloser::OnDialogDismissedByUser() {
  if (closing_profiles_.empty())
    return;

  if (dialog_)
    dialog_->Close();
  dialog_.reset();
  dialog_timer_.Stop();

  callbacks_.Notify(CloseResult::kAborted);
  closing_profiles_.clear();
}

void BrowserCloser::OnCloseSuccess(const base::FilePath& profile_dir) {
  if (!profile_dir.empty() && !base::Contains(closing_profiles_, profile_dir))
    return;  // Out of date.

  // TODO(crbug.com/1316551): Reset `closing_profiles_` if something weird
  // happens (e.g. new browser window is created by an extension).
  closing_profiles_.erase(profile_dir);
  if (!closing_profiles_.empty())
    return;  // There are profiles left to close still.

  callbacks_.Notify(CloseResult::kSuccess);
  closing_profiles_.clear();
}

void BrowserCloser::OnCloseAborted(const base::FilePath& profile_dir) {
  if (!base::Contains(closing_profiles_, profile_dir))
    return;  // Out of date.

  callbacks_.Notify(CloseResult::kAborted);
  closing_profiles_.clear();
}

}  // namespace enterprise_idle
