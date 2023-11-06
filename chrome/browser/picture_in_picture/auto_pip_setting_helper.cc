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
AutoPipSettingHelper::CreateForWebContents(content::WebContents* web_contents,
                                           base::OnceClosure close_pip_cb) {
  auto* settings_map = HostContentSettingsMapFactory::GetForProfile(
      web_contents->GetBrowserContext());
  auto* auto_blocker = PermissionDecisionAutoBlockerFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  return std::make_unique<AutoPipSettingHelper>(
      web_contents->GetLastCommittedURL(), settings_map, auto_blocker,
      std::move(close_pip_cb));
}

AutoPipSettingHelper::AutoPipSettingHelper(
    const GURL& origin,
    HostContentSettingsMap* settings_map,
    permissions::PermissionDecisionAutoBlockerBase* auto_blocker,
    base::OnceClosure close_pip_cb)
    : origin_(origin),
      settings_map_(settings_map),
      close_pip_cb_(std::move(close_pip_cb)),
      auto_blocker_(auto_blocker) {}

AutoPipSettingHelper::~AutoPipSettingHelper() = default;

void AutoPipSettingHelper::OnUserClosedWindow() {
  if (ui_was_shown_but_not_acknowledged_) {
    RecordResult(PromptResult::kIgnored);

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
  constraints.set_session_model(content_settings::SessionModel::Durable);

  settings_map_->SetContentSettingDefaultScope(
      origin_, /*secondary_url=*/GURL(),
      ContentSettingsType::AUTO_PICTURE_IN_PICTURE, new_setting, constraints);
}

AutoPipSettingHelper::ResultCb AutoPipSettingHelper::CreateResultCb() {
  weak_factory_.InvalidateWeakPtrs();
  return base::BindOnce(&AutoPipSettingHelper::OnUiResult,
                        weak_factory_.GetWeakPtr());
}

std::unique_ptr<AutoPipSettingOverlayView>
AutoPipSettingHelper::CreateOverlayViewIfNeeded(
    const gfx::Rect& browser_view_overridden_bounds,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow) {
  switch (GetEffectiveContentSetting()) {
    case CONTENT_SETTING_ASK:
      // Create and return the UI to ask the user.
      ui_was_shown_but_not_acknowledged_ = true;
      return std::make_unique<AutoPipSettingOverlayView>(
          CreateResultCb(), origin_, browser_view_overridden_bounds,
          anchor_view, arrow);
    case CONTENT_SETTING_ALLOW:
      // Nothing to do -- allow the auto pip to proceed.
      return nullptr;
    case CONTENT_SETTING_BLOCK:
      // Auto-pip is not allowed.  Close the window.
      std::move(close_pip_cb_).Run();
      return nullptr;
    default:
      NOTREACHED() << " AutoPiP unknown effective content setting";
      std::move(close_pip_cb_).Run();
      return nullptr;
  }
}

void AutoPipSettingHelper::IgnoreInputEvents(
    content::WebContents* web_contents) {
  CHECK(ui_was_shown_but_not_acknowledged_);
  scoped_ignore_input_events_ = web_contents->IgnoreInputEvents();
}

void AutoPipSettingHelper::OnUiResult(AutoPipSettingView::UiResult result) {
  // The UI was both shown and acknoweledged, so we don't have to worry about it
  // being dismissed without being acted on for the permission embargo.
  ui_was_shown_but_not_acknowledged_ = false;
  scoped_ignore_input_events_.reset();
  switch (result) {
    case AutoPipSettingView::UiResult::kBlock:
      RecordResult(PromptResult::kBlock);
      UpdateContentSetting(CONTENT_SETTING_BLOCK);
      // Also close the pip window.
      std::move(close_pip_cb_).Run();
      break;
    case AutoPipSettingView::UiResult::kAllowOnEveryVisit:
      RecordResult(PromptResult::kAllowOnEveryVisit);
      UpdateContentSetting(CONTENT_SETTING_ALLOW);
      break;
    case AutoPipSettingView::UiResult::kAllowOnce:
      RecordResult(PromptResult::kAllowOnce);
      // Leave at 'ASK'.  Do not update the embargo, since the user allowed the
      // feature to continue.  If anything, this should vote for 'anti-embargo'.
      break;
  }
}

void AutoPipSettingHelper::RecordResult(PromptResult result) {
  base::UmaHistogramEnumeration("Media.AutoPictureInPicture.PromptResult",
                                result);
}
