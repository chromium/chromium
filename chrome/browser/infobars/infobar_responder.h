// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INFOBARS_INFOBAR_RESPONDER_H_
#define CHROME_BROWSER_INFOBARS_INFOBAR_RESPONDER_H_

#include "base/memory/raw_ptr.h"
#include "components/infobars/core/infobar_manager.h"

namespace infobars {
class ContentInfoBarManager;
class InfoBar;
}

class ConfirmInfoBarDelegate;

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
  InfoBarResponder(infobars::ContentInfoBarManager* infobar_manager,
                   AutoResponseType response);

  InfoBarResponder(const InfoBarResponder&) = delete;
  InfoBarResponder& operator=(const InfoBarResponder&) = delete;

  ~InfoBarResponder() override;

  // infobars::InfoBarManager::Observer:
  void OnInfoBarAdded(infobars::InfoBar* infobar) override;
  void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                         infobars::InfoBar* new_infobar) override;

 private:
  void Respond(ConfirmInfoBarDelegate* delegate);

  raw_ptr<infobars::ContentInfoBarManager> infobar_manager_;
  AutoResponseType response_;
};

#endif  // CHROME_BROWSER_INFOBARS_INFOBAR_RESPONDER_H_
