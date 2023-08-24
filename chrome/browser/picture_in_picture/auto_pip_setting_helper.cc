// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_pip_setting_helper.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/web_contents.h"

// static
std::unique_ptr<AutoPipSettingHelper>
AutoPipSettingHelper::CreateForWebContents(content::WebContents* web_contents,
                                           base::OnceClosure close_pip_cb) {
  auto* settings_map = HostContentSettingsMapFactory::GetForProfile(
      web_contents->GetBrowserContext());
  return std::make_unique<AutoPipSettingHelper>(
      web_contents->GetLastCommittedURL(), settings_map,
      std::move(close_pip_cb));
}

AutoPipSettingHelper::AutoPipSettingHelper(const GURL& origin,
                                           HostContentSettingsMap* settings_map,
                                           base::OnceClosure close_pip_cb)
    : origin_(origin),
      settings_map_(settings_map),
      close_pip_cb_(std::move(close_pip_cb)) {}

AutoPipSettingHelper::~AutoPipSettingHelper() = default;

ContentSetting AutoPipSettingHelper::GetEffectiveContentSetting() {
  return settings_map_->GetContentSetting(
      origin_, /*secondary_url=*/GURL(),
      ContentSettingsType::AUTO_PICTURE_IN_PICTURE);
}

void AutoPipSettingHelper::UpdateContentSetting(ContentSetting new_setting) {
  content_settings::ContentSettingConstraints constraints;
  constraints.set_session_model(content_settings::SessionModel::Durable);

  settings_map_->SetContentSettingDefaultScope(
      origin_, /*secondary_url=*/GURL(),
      ContentSettingsType::AUTO_PICTURE_IN_PICTURE, new_setting, constraints);
}

std::unique_ptr<views::View> AutoPipSettingHelper::CreateOverlayViewIfNeeded() {
  switch (GetEffectiveContentSetting()) {
    case CONTENT_SETTING_ASK:
      // Create and return the UI to ask the user.
      return std::make_unique<AutoPipSettingOverlayView>(base::BindOnce(
          &AutoPipSettingHelper::OnUiResult, weak_factory_.GetWeakPtr()));
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

void AutoPipSettingHelper::OnUiResult(
    AutoPipSettingOverlayView::UiResult result) {
  switch (result) {
    case AutoPipSettingOverlayView::UiResult::kBlock:
      UpdateContentSetting(CONTENT_SETTING_BLOCK);
      // Also close the pip window.
      std::move(close_pip_cb_).Run();
      break;
    case AutoPipSettingOverlayView::UiResult::kAllow:
      UpdateContentSetting(CONTENT_SETTING_ALLOW);
      break;
    case AutoPipSettingOverlayView::UiResult::kDismissed:
      // Leave at 'ASK'.
      break;
  }
}
