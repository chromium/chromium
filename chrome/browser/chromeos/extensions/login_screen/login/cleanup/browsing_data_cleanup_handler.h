// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_BROWSING_DATA_CLEANUP_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_BROWSING_DATA_CLEANUP_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_handler.h"
#include "content/public/browser/browsing_data_remover.h"

namespace chromeos {

// A cleanup handler which clears the profile's browsing data using
// `BrowsingDataRemover`. See `chrome_browsing_data_remover::ALL_DATA_TYPES`
// for the list of data types removed.
class BrowsingDataCleanupHandler
    : public CleanupHandler,
      public content::BrowsingDataRemover::Observer {
 public:
  BrowsingDataCleanupHandler();
  ~BrowsingDataCleanupHandler() override;

  // CleanupHandler:
  void Cleanup(CleanupHandlerCallback callback) override;

  // content::BrowsingDataRemover::Observer:
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override;

 private:
  raw_ptr<content::BrowsingDataRemover> remover_;
  CleanupHandlerCallback callback_;
};

}  // namespace chromeos

#endif  //  CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_BROWSING_DATA_CLEANUP_HANDLER_H_
