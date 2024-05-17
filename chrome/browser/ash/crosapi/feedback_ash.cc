// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/feedback_ash.h"

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "components/user_manager/user_manager.h"

namespace crosapi {

namespace {

feedback::FeedbackSource FromMojo(mojom::LacrosFeedbackSource source) {
  switch (source) {
    case mojom::LacrosFeedbackSource::kLacrosBrowserCommand:
      return feedback::kFeedbackSourceBrowserCommand;
    case mojom::LacrosFeedbackSource::kLacrosSettingsAboutPage:
      return feedback::kFeedbackSourceMdSettingsAboutPage;
    case mojom::LacrosFeedbackSource::kLacrosAutofillContextMenu:
      return feedback::kFeedbackSourceAutofillContextMenu;
    case mojom::LacrosFeedbackSource::kLacrosSadTabPage:
      return feedback::kFeedbackSourceSadTabPage;
    case mojom::LacrosFeedbackSource::kLacrosChromeLabs:
      return feedback::kFeedbackSourceChromeLabs;
    case mojom::LacrosFeedbackSource::kLacrosQuickAnswers:
      return feedback::kFeedbackSourceQuickAnswers;
    case mojom::LacrosFeedbackSource::kDeprecatedLacrosWindowLayoutMenu:
      return feedback::kFeedbackSourceWindowLayoutMenu;
    case mojom::LacrosFeedbackSource::kFeedbackSourceCookieControls:
      return feedback::kFeedbackSourceCookieControls;
    case mojom::LacrosFeedbackSource::kFeedbackSourceSettingsPerformancePage:
      return feedback::kFeedbackSourceSettingsPerformancePage;
    case mojom::LacrosFeedbackSource::kFeedbackSourceProfileErrorDialog:
      return feedback::kFeedbackSourceProfileErrorDialog;
    case mojom::LacrosFeedbackSource::kFeedbackSourceQuickOffice:
      return feedback::kFeedbackSourceQuickOffice;
    case mojom::LacrosFeedbackSource::kFeedbackSourceAI:
      return feedback::kFeedbackSourceAI;
    case mojom::LacrosFeedbackSource::kFeedbackSourceLensOverlay:
      return feedback::kFeedbackSourceLensOverlay;
    case mojom::LacrosFeedbackSource::kUnknown:
      return feedback::kFeedbackSourceUnknownLacrosSource;
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
    if (!feedback_info->autofill_metadata->is_dict()) {
      LOG(ERROR) << "Feedback info autofill metadata is not a dict.";
      return;
    }
    autofill_metadata = std::move(*feedback_info->autofill_metadata).TakeDict();
  }
  base::Value::Dict ai_metadata;
  if (feedback_info->ai_metadata) {
    if (!feedback_info->ai_metadata->is_dict()) {
      LOG(ERROR) << "Feedback info ai metadata is not a dict.";
      return;
    }
    ai_metadata = std::move(*feedback_info->ai_metadata).TakeDict();
  }
  chrome::ShowFeedbackPage(
      feedback_info->page_url, profile, FromMojo(feedback_info->source),
      feedback_info->description_template,
      feedback_info->description_placeholder_text, feedback_info->category_tag,
      feedback_info->extra_diagnostics, std::move(autofill_metadata),
      std::move(ai_metadata));
}

}  // namespace crosapi
