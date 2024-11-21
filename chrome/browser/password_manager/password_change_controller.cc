// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change_controller.h"

#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

PasswordChangeController::PasswordChangeController(
    GURL change_password_url,
    std::u16string username,
    std::u16string password,
    content::WebContents* originator)
    : change_password_url_(std::move(change_password_url)),
      username_(std::move(username)),
      original_password_(std::move(password)),
      originator_(originator->GetWeakPtr()) {
  content::WebContents* new_tab = originator_->OpenURL(
      content::OpenURLParams(change_password_url_, content::Referrer(),
                             WindowOpenDisposition::NEW_BACKGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK,
                             /*is_renderer_initiated=*/false),
      base::DoNothing());
  if (new_tab) {
    executor_ = new_tab->GetWeakPtr();
  }
}

PasswordChangeController::~PasswordChangeController() = default;
