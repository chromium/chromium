// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_BROWSER_INFOBAR_MANAGER_H_
#define CHROME_BROWSER_INFOBARS_BROWSER_INFOBAR_MANAGER_H_

#include <map>
#include <memory>

#include "chrome/browser/infobars/infobar_spec.h"
#include "components/infobars/core/infobar_delegate.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserProcess;

namespace infobars {

// BrowserInfoBarManager handles the lifecycle and scope management of modern
// InfoBars. Registered as a GlobalFeature.
class BrowserInfoBarManager {
 public:
  explicit BrowserInfoBarManager(BrowserProcess* browser_process);
  BrowserInfoBarManager(const BrowserInfoBarManager&) = delete;
  BrowserInfoBarManager& operator=(const BrowserInfoBarManager&) = delete;
  ~BrowserInfoBarManager();

  DECLARE_USER_DATA(BrowserInfoBarManager);

  static BrowserInfoBarManager* From(BrowserProcess* browser_process);

  // Registers an InfoBarSpec with the manager.
  void Register(InfoBarSpec spec);

  // Shows the InfoBar with the given identifier.
  void Show(infobars::InfoBarDelegate::InfoBarIdentifier identifier);

  // Hides the InfoBar with the given identifier.
  void Hide(infobars::InfoBarDelegate::InfoBarIdentifier identifier);

 private:
  // Returns the approved priority for an InfoBar.
  InfoBarPriority GetApprovedPriority(
      infobars::InfoBarDelegate::InfoBarIdentifier identifier);
  ui::ScopedUnownedUserData<BrowserInfoBarManager> scoped_unowned_user_data_;

  std::map<infobars::InfoBarDelegate::InfoBarIdentifier, InfoBarSpec>
      registered_specs_;
};

}  // namespace infobars

#endif  // CHROME_BROWSER_INFOBARS_BROWSER_INFOBAR_MANAGER_H_
