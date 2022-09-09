// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/feedback_ash.h"

#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
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

FeedbackAsh::FeedbackAsh() = default;

FeedbackAsh::~FeedbackAsh() = default;

void FeedbackAsh::BindReceiver(
    mojo::PendingReceiver<mojom::Feedback> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FeedbackAsh::ShowFeedbackPage(mojom::FeedbackInfoPtr feedback_info) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user) {
    LOG(ERROR) << "Cannot invoke feedback for lacros: No primary user found!";
    return;
  }
  Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(user);
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
