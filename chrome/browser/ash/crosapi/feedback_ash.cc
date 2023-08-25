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
    case mojom::LacrosFeedbackSource::kLacrosAutofillContextMenu:
      return chrome::kFeedbackSourceAutofillContextMenu;
    case mojom::LacrosFeedbackSource::kLacrosSadTabPage:
      return chrome::kFeedbackSourceSadTabPage;
    case mojom::LacrosFeedbackSource::kLacrosChromeLabs:
      return chrome::kFeedbackSourceChromeLabs;
    case mojom::LacrosFeedbackSource::kLacrosQuickAnswers:
      return chrome::kFeedbackSourceQuickAnswers;
    case mojom::LacrosFeedbackSource::kDeprecatedLacrosWindowLayoutMenu:
      return chrome::kFeedbackSourceWindowLayoutMenu;
    case mojom::LacrosFeedbackSource::kFeedbackSourceCookieControls:
      return chrome::kFeedbackSourceCookieControls;
    case mojom::LacrosFeedbackSource::kFeedbackSourceSettingsPerformancePage:
      return chrome::kFeedbackSourceSettingsPerformancePage;
    case mojom::LacrosFeedbackSource::kFeedbackSourceProfileErrorDialog:
      return chrome::kFeedbackSourceProfileErrorDialog;
    case mojom::LacrosFeedbackSource::kFeedbackSourceQuickOffice:
      return chrome::kFeedbackSourceQuickOffice;
    case mojom::LacrosFeedbackSource::kUnknown:
      return chrome::kFeedbackSourceUnknownLacrosSource;
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
  base::Value::Dict autofill_metadata;
  if (feedback_info->autofill_metadata) {
    DCHECK(feedback_info->autofill_metadata->is_dict());
    autofill_metadata = std::move(*feedback_info->autofill_metadata).TakeDict();
  }
  chrome::ShowFeedbackPage(
      feedback_info->page_url, profile, FromMojo(feedback_info->source),
      feedback_info->description_template,
      feedback_info->description_placeholder_text, feedback_info->category_tag,
      feedback_info->extra_diagnostics, std::move(autofill_metadata));
}

}  // namespace crosapi
