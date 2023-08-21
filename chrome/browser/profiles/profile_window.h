// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_WINDOW_H_
#define CHROME_BROWSER_PROFILES_PROFILE_WINDOW_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_types.h"

#if BUILDFLAG(IS_ANDROID)
#error "Not used on Android"
#endif

class BrowserList;
class Profile;

namespace base {
class FilePath;
}

namespace profiles {

// Activates a window for |profile| on the desktop specified by
// |desktop_type|. If no such window yet exists, or if |always_create| is
// true, this first creates a new window, then activates
// that. If activating an exiting window and multiple windows exists then the
// window that was most recently active is activated. This is used for
// creation of a window from the multi-profile dropdown menu.
void FindOrCreateNewWindowForProfile(
    Profile* profile,
    chrome::startup::IsProcessStartup process_startup,
    chrome::startup::IsFirstRun is_first_run,
    bool always_create);

// Opens a Browser for |profile|.
// If |always_create| is true a window is created even if one already exists.
// If |is_new_profile| is true a first run window is created.
// If |unblock_extensions| is true, all extensions are unblocked.
// |callback| is called with a nullptr `Browser` in case of failure.
// |callback| may be null.
void OpenBrowserWindowForProfile(base::OnceCallback<void(Browser*)> callback,
                                 bool always_create,
                                 bool is_new_profile,
                                 bool unblock_extensions,
                                 Profile* profile);

// Loads the specified profile given by |path| asynchronously. Once profile is
// loaded and initialized it runs |callback| if it isn't null.
void LoadProfileAsync(const base::FilePath& path,
                      base::OnceCallback<void(Profile*)> callback);

// Opens a Browser with the specified profile given by |path|.
// If |always_create| is true then a new window is created
// even if a window for that profile already exists. When the browser is
// opened, |callback| will be run if it isn't null.
void SwitchToProfile(const base::FilePath& path,
                     bool always_create,
                     base::OnceCallback<void(Browser*)> callback =
                         base::OnceCallback<void(Browser*)>());

// Opens a Browser for the guest profile and runs |callback| if it isn't null.
void SwitchToGuestProfile(base::OnceCallback<void(Browser*)> callback =
                              base::OnceCallback<void(Browser*)>());

// Returns true if |profile| has potential profile switch targets, ie there's at
// least one other profile available to switch to, not counting guest. This is
// the case when there are more than 1 profiles available or when there's only
// one and the current window is a guest window.
bool HasProfileSwitchTargets(Profile* profile);

// Close all the browser windows for |profile|.
void CloseProfileWindows(Profile* profile);

// Handles running a callback when a new Browser for the given profile
// has been completely created.  This object deletes itself once the browser
// is created and the callback is executed.
// Warning: this may be called with a nullptr Browser in case of failure (e.g.
// if the profile or the browser is destroyed during the operation).
class BrowserAddedForProfileObserver : public BrowserListObserver {
 public:
  BrowserAddedForProfileObserver(Profile* profile,
                                 base::OnceCallback<void(Browser*)> callback);
  ~BrowserAddedForProfileObserver() override;

  BrowserAddedForProfileObserver(const BrowserAddedForProfileObserver&) =
      delete;
  BrowserAddedForProfileObserver& operator=(
      const BrowserAddedForProfileObserver&) = delete;

 private:
  // Overridden from BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  void NotifyBrowserCreatedAnDie();

  // Profile for which the browser should be opened.
  base::WeakPtr<Profile> profile_;
  raw_ptr<Browser> browser_ = nullptr;
  base::OnceCallback<void(Browser*)> callback_;
  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};
};

}  // namespace profiles

#endif  // CHROME_BROWSER_PROFILES_PROFILE_WINDOW_H_
