// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_CHROMEOS_H_
#define CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_CHROMEOS_H_

#include "base/callback_list.h"
#include "base/sequence_checker.h"
#include "chrome/browser/browser_process_platform_part_base.h"
#include "chrome/browser/ui/browser_list_observer.h"

class Browser;
class Profile;

class BrowserProcessPlatformPartChromeOS
    : public BrowserProcessPlatformPartBase {
 public:
  BrowserProcessPlatformPartChromeOS();

  BrowserProcessPlatformPartChromeOS(
      const BrowserProcessPlatformPartChromeOS&) = delete;
  BrowserProcessPlatformPartChromeOS& operator=(
      const BrowserProcessPlatformPartChromeOS&) = delete;

  ~BrowserProcessPlatformPartChromeOS() override;

 protected:
  // Returns true if we can restore URLs for `profile`. Restoring URLs should
  // only be allowed for regular signed-in users. This is currently virtual as
  // lacros-chrome and ash-chrome check this in different ways.
  // TODO(tluk): Have both ash-chrome and lacros-chrome share the same profile
  // check code.
  virtual bool CanRestoreUrlsForProfile(const Profile* profile) const;

 private:
  // An observer that restores urls based on the on startup setting after a new
  // browser is added to the BrowserList.
  class BrowserRestoreObserver : public BrowserListObserver {
   public:
    explicit BrowserRestoreObserver(const BrowserProcessPlatformPartChromeOS*
                                        browser_process_platform_part);

    ~BrowserRestoreObserver() override;

   protected:
    // BrowserListObserver:
    void OnBrowserAdded(Browser* browser) override;

   private:
    // Returns true, if the url defined in the on startup setting should be
    // opened. Otherwise, returns false.
    bool ShouldRestoreUrls(Browser* browser) const;

    // Returns true, if the url defined in the on startup setting should be
    // opened in a new browser. Otherwise, returns false.
    bool ShouldOpenUrlsInNewBrowser(Browser* browser) const;

    // Restores urls based on the on startup setting.
    void RestoreUrls(Browser* browser);

    // Called when a session is restored.
    void OnSessionRestoreDone(Profile* profile, int num_tabs_restored);

    const raw_ptr<const BrowserProcessPlatformPartChromeOS>
        browser_process_platform_part_;

    base::CallbackListSubscription on_session_restored_callback_subscription_;
  };

  BrowserRestoreObserver browser_restore_observer_;
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_CHROMEOS_H_
