// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_BROWSER_CLEANUP_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_BROWSER_CLEANUP_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "content/public/browser/browsing_data_remover.h"

namespace chromeos {

// A cleanup handler that first closes all browser windows and then clears
// the profile's browsing data using `BrowsingDataRemover`. See
// `chrome_browsing_data_remover::ALL_DATA_TYPES` for the list of data types
// removed.
class BrowserCleanupHandler : public CleanupHandler,
                              public BrowserListObserver,
                              public content::BrowsingDataRemover::Observer {
 public:
  BrowserCleanupHandler();
  ~BrowserCleanupHandler() override;

  // CleanupHandler:
  void Cleanup(CleanupHandlerCallback callback) override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;

  void RemoveBrowserHistory();

  // content::BrowsingDataRemover::Observer:
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override;

 private:
  raw_ptr<content::BrowsingDataRemover> data_remover_;
  CleanupHandlerCallback callback_;
  raw_ptr<Profile> profile_;
};

}  // namespace chromeos

#endif  //  CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_BROWSER_CLEANUP_HANDLER_H_
