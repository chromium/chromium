// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_pip_setting_helper.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/picture_in_picture/auto_pip_setting_overlay_view.h"
#endif

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

#if !BUILDFLAG(IS_ANDROID)
void AutoPipSettingHelper::OnUserClosedWindow(
    media::PictureInPictureEventsInfo::AutoPipReason auto_pip_reason,
    std::optional<ukm::SourceId> source_id) {
  if (ui_was_shown_but_not_acknowledged_) {
    RecordResult(PromptResult::kIgnored, auto_pip_reason, std::move(source_id));

    // Usually, this isn't needed, since any later pip window that reuses us
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
#endif  // !BUILDFLAG(IS_ANDROID)

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

  // Enable last-visit tracking for eligible permissions granted from
  // Auto PiP bubble. This allows Safety Hub to auto-revoke the permission
  // if the site is not visited for a finite amount of time.
  if (base::FeatureList::IsEnabled(
          permissions::features::
              kSafetyHubUnusedPermissionRevocationForAllSurfaces) &&
      new_setting &&
      content_settings::CanBeAutoRevokedAsUnusedPermission(
          ContentSettingsType::AUTO_PICTURE_IN_PICTURE,
          content_settings::ContentSettingToValue(new_setting))) {
    constraints.set_track_last_visit_for_autoexpiration(true);
  }

  settings_map_->SetContentSettingDefaultScope(
      origin_, /*secondary_url=*/GURL(),
      ContentSettingsType::AUTO_PICTURE_IN_PICTURE, new_setting, constraints);
}

#if !BUILDFLAG(IS_ANDROID)
AutoPipSettingHelper::ResultCb AutoPipSettingHelper::CreateResultCb(
    base::OnceClosure close_pip_cb,
    media::PictureInPictureEventsInfo::AutoPipReason auto_pip_reason,
    std::optional<ukm::SourceId> source_id) {
  weak_factory_.InvalidateWeakPtrs();
  return base::BindOnce(&AutoPipSettingHelper::OnUiResult,
                        weak_factory_.GetWeakPtr(), std::move(close_pip_cb),
                        auto_pip_reason, std::move(source_id));
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
std::unique_ptr<AutoPipSettingOverlayView>
AutoPipSettingHelper::CreateOverlayViewIfNeeded(
    base::OnceClosure close_pip_cb,
    media::PictureInPictureEventsInfo::AutoPipReason auto_pip_reason,
    std::optional<ukm::SourceId> source_id,
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow) {
  switch (GetEffectiveContentSetting()) {
    case CONTENT_SETTING_ASK:
      // If the user already said to allow once, then continue allowing.  It's
      // assumed that we're used for at most one visit to a site.
      if (already_selected_allow_once_) {
        RecordResult(PromptResult::kNotShownAllowedOnce, auto_pip_reason,
                     std::move(source_id));
        return nullptr;
      }
      // Create and return the UI to ask the user.
      ui_was_shown_but_not_acknowledged_ = true;
      return std::make_unique<AutoPipSettingOverlayView>(
          CreateResultCb(std::move(close_pip_cb), auto_pip_reason,
                         std::move(source_id)),
          origin_, anchor_view, arrow);
    case CONTENT_SETTING_ALLOW:
      // Nothing to do -- allow the auto pip to proceed.
      RecordResult(PromptResult::kNotShownAllowedOnEveryVisit, auto_pip_reason,
                   std::move(source_id));
      return nullptr;
    case CONTENT_SETTING_BLOCK:
      // Auto-pip is not allowed.  Close the window.
      RecordResult(PromptResult::kNotShownBlocked, auto_pip_reason,
                   std::move(source_id));
      std::move(close_pip_cb).Run();
      return nullptr;
    default:
      NOTREACHED() << " AutoPiP unknown effective content setting";
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

void AutoPipSettingHelper::OnAutoPipBlockedByPermission(
    media::PictureInPictureEventsInfo::AutoPipReason auto_pip_reason,
    std::optional<ukm::SourceId> source_id) {
  RecordResult(PromptResult::kNotShownBlocked, auto_pip_reason,
               std::move(source_id));
}

void AutoPipSettingHelper::OnAutoPipBlockedByIncognito(
    media::PictureInPictureEventsInfo::AutoPipReason auto_pip_reason) {
  RecordResult(PromptResult::kNotShownIncognito, auto_pip_reason, std::nullopt);
}

#if !BUILDFLAG(IS_ANDROID)
void AutoPipSettingHelper::OnUiResult(
    base::OnceClosure close_pip_cb,
    media::PictureInPictureEventsInfo::AutoPipReason auto_pip_reason,
    std::optional<ukm::SourceId> source_id,
    AutoPipSettingView::UiResult result) {
  // The UI was both shown and acknowledged, so we don't have to worry about it
  // being dismissed without being acted on for the permission embargo.
  ui_was_shown_but_not_acknowledged_ = false;
  switch (result) {
    case AutoPipSettingView::UiResult::kBlock:
      RecordResult(PromptResult::kBlock, auto_pip_reason, std::move(source_id));
      UpdateContentSetting(CONTENT_SETTING_BLOCK);
      // Also close the pip window.
      std::move(close_pip_cb).Run();
      break;
    case AutoPipSettingView::UiResult::kAllowOnEveryVisit:
      RecordResult(PromptResult::kAllowOnEveryVisit, auto_pip_reason,
                   std::move(source_id));
      UpdateContentSetting(CONTENT_SETTING_ALLOW);
      break;
    case AutoPipSettingView::UiResult::kAllowOnce:
      already_selected_allow_once_ = true;
      RecordResult(PromptResult::kAllowOnce, auto_pip_reason,
                   std::move(source_id));
      // Leave at 'ASK'.  Do not update the embargo, since the user allowed the
      // feature to continue.  If anything, this should vote for 'anti-embargo'.
      break;
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

void AutoPipSettingHelper::RecordResult(
    PromptResult result,
    media::PictureInPictureEventsInfo::AutoPipReason auto_pip_reason,
    std::optional<ukm::SourceId> source_id) {
  base::UmaHistogramEnumeration("Media.AutoPictureInPicture.PromptResultV2",
                                result);
  switch (auto_pip_reason) {
    case media::PictureInPictureEventsInfo::AutoPipReason::kUnknown:
      break;
    case media::PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing:
      base::UmaHistogramEnumeration(
          "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReason."
          "VideoConferencing.PromptResultV2",
          result);
      base::UmaHistogramEnumeration(
          "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
          "VideoConferencing.PromptResultV2",
          result);
      RecordUkms(auto_pip_reason, source_id, result);
      break;
    case media::PictureInPictureEventsInfo::AutoPipReason::kMediaPlayback:
      base::UmaHistogramEnumeration(
          "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReason."
          "MediaPlayback.PromptResultV2",
          result);
      base::UmaHistogramEnumeration(
          "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
          "MediaPlayback.PromptResultV2",
          result);
      RecordUkms(auto_pip_reason, source_id, result);
      break;
    case media::PictureInPictureEventsInfo::AutoPipReason::kBrowserInitiated:
      base::UmaHistogramEnumeration(
          "Media.AutoPictureInPicture.EnterPictureInPicture.AutomaticReasonV2."
          "BrowserInitiated.PromptResultV2",
          result);
      RecordUkms(auto_pip_reason, source_id, result);
      break;
  }
}

void AutoPipSettingHelper::RecordUkms(
    media::PictureInPictureEventsInfo::AutoPipReason auto_pip_reason,
    std::optional<ukm::SourceId> source_id,
    PromptResult result) const {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder || !source_id) {
    return;
  }

  switch (auto_pip_reason) {
    case media::PictureInPictureEventsInfo::AutoPipReason::kUnknown:
      break;
    case media::PictureInPictureEventsInfo::AutoPipReason::kVideoConferencing:
      ukm::builders::
          Media_AutoPictureInPicture_EnterPictureInPicture_AutomaticReason_PromptResultV2(
              source_id.value())
              .SetVideoConferencing(static_cast<uintmax_t>(result))
              .Record(ukm_recorder);
      break;
    case media::PictureInPictureEventsInfo::AutoPipReason::kMediaPlayback:
      ukm::builders::
          Media_AutoPictureInPicture_EnterPictureInPicture_AutomaticReason_PromptResultV2(
              source_id.value())
              .SetMediaPlayback(static_cast<uintmax_t>(result))
              .Record(ukm_recorder);
      break;
    case media::PictureInPictureEventsInfo::AutoPipReason::kBrowserInitiated:
      ukm::builders::
          Media_AutoPictureInPicture_EnterPictureInPicture_AutomaticReason_PromptResultV2(
              source_id.value())
              .SetBrowserInitiated(static_cast<uintmax_t>(result))
              .Record(ukm_recorder);
      break;
  }
}
