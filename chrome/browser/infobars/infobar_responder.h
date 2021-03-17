// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_RESPONDER_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_RESPONDER_H_

#include "base/macros.h"
#include "components/infobars/core/infobar_manager.h"

namespace infobars {
class InfoBar;
}

class ConfirmInfoBarDelegate;
class InfoBarService;

// Used by test code to asynchronously respond to the first infobar shown, which
// must have a ConfirmInfoBarDelegate. This can be used to ensure various
// interaction flows work correctly.
//
// The asynchronous response matches how real users will use the infobar.
class InfoBarResponder : public infobars::InfoBarManager::Observer {
 public:
  enum AutoResponseType {
    ACCEPT,
    DENY,
    DISMISS
  };

  // The responder will asynchronously perform the requested |response|.
  InfoBarResponder(InfoBarService* infobar_service, AutoResponseType response);
  ~InfoBarResponder() override;

  // infobars::InfoBarManager::Observer:
  void OnInfoBarAdded(infobars::InfoBar* infobar) override;
  void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                         infobars::InfoBar* new_infobar) override;

 private:
  void Respond(ConfirmInfoBarDelegate* delegate);

  InfoBarService* infobar_service_;
  AutoResponseType response_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarResponder);
};

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_RESPONDER_H_
