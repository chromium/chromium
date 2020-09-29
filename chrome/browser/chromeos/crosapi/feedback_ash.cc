// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/feedback_ash.h"

#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/chrome_pages.h"

namespace crosapi {

namespace {

chrome::FeedbackSource FromMojo(mojom::LacrosFeedbackSource source) {
  switch (source) {
    case mojom::LacrosFeedbackSource::kLacrosBrowserCommand:
      return chrome::kFeedbackSourceBrowserCommand;
    case mojom::LacrosFeedbackSource::kLacrosSettingsAboutPage:
      return chrome::kFeedbackSourceMdSettingsAboutPage;
  }
}

}  // namespace

FeedbackAsh::FeedbackAsh(mojo::PendingReceiver<mojom::Feedback> receiver)
    : receiver_(this, std::move(receiver)) {}

FeedbackAsh::~FeedbackAsh() = default;

void FeedbackAsh::ShowFeedbackPage(mojom::FeedbackInfoPtr feedback_info) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user) {
    LOG(ERROR) << "Cannot invoke feedback for lacros: No primary user found!";
    return;
  }
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile) {
    LOG(ERROR)
        << "Cannot invoke feedback for lacros: No primary profile found!";
    return;
  }
  chrome::ShowFeedbackPage(
      feedback_info->page_url, profile, FromMojo(feedback_info->source),
      feedback_info->description_template,
      feedback_info->description_placeholder_text, feedback_info->category_tag,
      feedback_info->extra_diagnostics);
}

}  // namespace crosapi
