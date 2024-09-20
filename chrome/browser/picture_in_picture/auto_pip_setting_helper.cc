// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_pip_setting_helper.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "content/public/browser/web_contents.h"

// static
std::unique_ptr<AutoPipSettingHelper>
AutoPipSettingHelper::CreateForWebContents(
    content::WebContents* web_contents,
    HostContentSettingsMap* settings_map,
    permissions::PermissionDecisionAutoBlockerBase* auto_blocker) {
  return std::make_unique<AutoPipSettingHelper>(
      web_contents->GetLastCommittedURL(), settings_map, auto_blocker);
}

AutoPipSettingHelper::AutoPipSettingHelper(
    const GURL& origin,
    HostContentSettingsMap* settings_map,
    permissions::PermissionDecisionAutoBlockerBase* auto_blocker)
    : origin_(origin),
      settings_map_(settings_map),
      auto_blocker_(auto_blocker) {}

AutoPipSettingHelper::~AutoPipSettingHelper() = default;

void AutoPipSettingHelper::OnUserClosedWindow() {
  if (ui_was_shown_but_not_acknowledged_) {
    RecordResult(PromptResult::kIgnored);

    // Usually, this isn't needed, since any later pip window that re-uses us
    // will be for the same site and will still be set to 'ASK'.  In that case,
    // we'll show the permission UI.  However, if the permission changes out
    // from under us somehow (e.g., the user sets it to allow via the permission
    // chip), then future windows might not show the prompt.  This ensures that
    // closing those windows, which were allowed, don't fiddle with the embargo.
    // It doesn't really matter, but for completeness we do it.
    ui_was_shown_but_not_acknowledged_ = false;

    if (auto_blocker_) {
      auto_blocker_->RecordDismissAndEmbargo(
          origin_, ContentSettingsType::AUTO_PICTURE_IN_PICTURE,
          /*dismissed_prompt_was_quiet=*/false);
    }
  }
}

ContentSetting AutoPipSettingHelper::GetEffectiveContentSetting() {
  auto setting = settings_map_->GetContentSetting(
      origin_, /*secondary_url=*/GURL(),
      ContentSettingsType::AUTO_PICTURE_IN_PICTURE);

  if (setting == CONTENT_SETTING_ASK && auto_blocker_) {
    if (auto_blocker_->IsEmbargoed(
            origin_, ContentSettingsType::AUTO_PICTURE_IN_PICTURE)) {
      return CONTENT_SETTING_BLOCK;
    }
  }

  return setting;
}

void AutoPipSettingHelper::UpdateContentSetting(ContentSetting new_setting) {
  content_settings::ContentSettingConstraints constraints;
  constraints.set_session_model(content_settings::mojom::SessionModel::DURABLE);

  settings_map_->SetContentSettingDefaultScope(
      origin_, /*secondary_url=*/GURL(),
      ContentSettingsType::AUTO_PICTURE_IN_PICTURE, new_setting, constraints);
}

AutoPipSettingHelper::ResultCb AutoPipSettingHelper::CreateResultCb(
    base::OnceClosure close_pip_cb) {
  weak_factory_.InvalidateWeakPtrs();
  return base::BindOnce(&AutoPipSettingHelper::OnUiResult,
                        weak_factory_.GetWeakPtr(), std::move(close_pip_cb));
}

std::unique_ptr<AutoPipSettingOverlayView>
AutoPipSettingHelper::CreateOverlayViewIfNeeded(
    base::OnceClosure close_pip_cb,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow) {
  switch (GetEffectiveContentSetting()) {
    case CONTENT_SETTING_ASK:
      // If the user already said to allow once, then continue allowing.  It's
      // assumed that we're used for at most one visit to a site.
      if (already_selected_allow_once_) {
        RecordResult(PromptResult::kNotShownAllowedOnce);
        return nullptr;
      }
      // Create and return the UI to ask the user.
      ui_was_shown_but_not_acknowledged_ = true;
      return std::make_unique<AutoPipSettingOverlayView>(
          CreateResultCb(std::move(close_pip_cb)), origin_, anchor_view, arrow);
    case CONTENT_SETTING_ALLOW:
      // Nothing to do -- allow the auto pip to proceed.
      RecordResult(PromptResult::kNotShownAllowedOnEveryVisit);
      return nullptr;
    case CONTENT_SETTING_BLOCK:
      // Auto-pip is not allowed.  Close the window.
      RecordResult(PromptResult::kNotShownBlocked);
      std::move(close_pip_cb).Run();
      return nullptr;
    default:
      NOTREACHED_IN_MIGRATION() << " AutoPiP unknown effective content setting";
      std::move(close_pip_cb).Run();
      return nullptr;
  }
}

void AutoPipSettingHelper::OnAutoPipBlockedByPermission() {
  RecordResult(PromptResult::kNotShownBlocked);
}

void AutoPipSettingHelper::OnAutoPipBlockedByIncognito() {
  RecordResult(PromptResult::kNotShownIncognito);
}

void AutoPipSettingHelper::OnUiResult(base::OnceClosure close_pip_cb,
                                      AutoPipSettingView::UiResult result) {
  // The UI was both shown and acknowledged, so we don't have to worry about it
  // being dismissed without being acted on for the permission embargo.
  ui_was_shown_but_not_acknowledged_ = false;
  switch (result) {
    case AutoPipSettingView::UiResult::kBlock:
      RecordResult(PromptResult::kBlock);
      UpdateContentSetting(CONTENT_SETTING_BLOCK);
      // Also close the pip window.
      std::move(close_pip_cb).Run();
      break;
    case AutoPipSettingView::UiResult::kAllowOnEveryVisit:
      RecordResult(PromptResult::kAllowOnEveryVisit);
      UpdateContentSetting(CONTENT_SETTING_ALLOW);
      break;
    case AutoPipSettingView::UiResult::kAllowOnce:
      already_selected_allow_once_ = true;
      RecordResult(PromptResult::kAllowOnce);
      // Leave at 'ASK'.  Do not update the embargo, since the user allowed the
      // feature to continue.  If anything, this should vote for 'anti-embargo'.
      break;
  }
}

void AutoPipSettingHelper::RecordResult(PromptResult result) {
  base::UmaHistogramEnumeration("Media.AutoPictureInPicture.PromptResultV2",
                                result);
}
