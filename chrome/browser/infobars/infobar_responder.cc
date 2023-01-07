// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar_responder.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"

InfoBarResponder::InfoBarResponder(
    infobars::ContentInfoBarManager* infobar_manager,
    AutoResponseType response)
    : infobar_manager_(infobar_manager), response_(response) {
  infobar_manager_->AddObserver(this);
}

InfoBarResponder::~InfoBarResponder() {
  // This is safe even if we were already removed as an observer in
  // OnInfoBarAdded().
  infobar_manager_->RemoveObserver(this);
}

void InfoBarResponder::OnInfoBarAdded(infobars::InfoBar* infobar) {
  infobar_manager_->RemoveObserver(this);
  ConfirmInfoBarDelegate* delegate =
      infobar->delegate()->AsConfirmInfoBarDelegate();
  DCHECK(delegate);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&InfoBarResponder::Respond,
                                base::Unretained(this), delegate));
}

void InfoBarResponder::OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                                         infobars::InfoBar* new_infobar) {
  OnInfoBarAdded(new_infobar);
}

void InfoBarResponder::Respond(ConfirmInfoBarDelegate* delegate) {
  switch (response_) {
    case ACCEPT:
      delegate->Accept();
      break;
    case DENY:
      delegate->Cancel();
      break;
    case DISMISS:
      delegate->InfoBarDismissed();
      break;
  }
}
