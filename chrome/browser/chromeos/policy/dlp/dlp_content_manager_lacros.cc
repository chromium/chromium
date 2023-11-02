// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager_lacros.h"

#include "base/containers/cxx20_erase.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chromeos/crosapi/mojom/dlp.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/platform_window/platform_window.h"

namespace policy {

namespace {

crosapi::mojom::DlpRestrictionLevel ConvertLevelToMojo(
    DlpRulesManager::Level level) {
  switch (level) {
    case DlpRulesManager::Level::kReport:
      return crosapi::mojom::DlpRestrictionLevel::kReport;
    case DlpRulesManager::Level::kWarn:
      return crosapi::mojom::DlpRestrictionLevel::kWarn;
    case DlpRulesManager::Level::kBlock:
      return crosapi::mojom::DlpRestrictionLevel::kBlock;
    case DlpRulesManager::Level::kAllow:
      return crosapi::mojom::DlpRestrictionLevel::kAllow;
    case DlpRulesManager::Level::kNotSet:
      return crosapi::mojom::DlpRestrictionLevel::kNotSet;
  }
}

crosapi::mojom::DlpRestrictionLevelAndUrlPtr ConvertLevelAndUrlToMojo(
    RestrictionLevelAndUrl level_and_url) {
  auto result = crosapi::mojom::DlpRestrictionLevelAndUrl::New();
  result->level = ConvertLevelToMojo(level_and_url.level);
  result->url = level_and_url.url;
  return result;
}

crosapi::mojom::DlpRestrictionSetPtr ConvertRestrictionSetToMojo(
    const DlpContentRestrictionSet& restriction_set) {
  auto result = crosapi::mojom::DlpRestrictionSet::New();
  result->screenshot =
      ConvertLevelAndUrlToMojo(restriction_set.GetRestrictionLevelAndUrl(
          DlpContentRestriction::kScreenshot));
  result->privacy_screen =
      ConvertLevelAndUrlToMojo(restriction_set.GetRestrictionLevelAndUrl(
          DlpContentRestriction::kPrivacyScreen));
  result->print = ConvertLevelAndUrlToMojo(
      restriction_set.GetRestrictionLevelAndUrl(DlpContentRestriction::kPrint));
  result->screen_share =
      ConvertLevelAndUrlToMojo(restriction_set.GetRestrictionLevelAndUrl(
          DlpContentRestriction::kScreenShare));
  return result;
}

crosapi::mojom::ScreenShareAreaPtr ConvertToScreenShareArea(
    const content::DesktopMediaID& media_id) {
  auto result = crosapi::mojom::ScreenShareArea::New();
  if (media_id.type == content::DesktopMediaID::Type::TYPE_SCREEN) {
    return result;
  }
  DCHECK_EQ(media_id.type, content::DesktopMediaID::Type::TYPE_WINDOW);
  aura::Window* window = content::DesktopMediaID::GetNativeWindowById(media_id);
  if (window) {
    result->window_id = lacros_window_utility::GetRootWindowUniqueId(window);
  }
  result->snapshot_source_id = media_id.id;
  return result;
}

}  // namespace

// static
DlpContentManagerLacros* DlpContentManagerLacros::Get() {
  return static_cast<DlpContentManagerLacros*>(DlpContentObserver::Get());
}

void DlpContentManagerLacros::CheckScreenShareRestriction(
    const content::DesktopMediaID& media_id,
    const std::u16string& application_title,
    OnDlpRestrictionCheckedCallback callback) {
  if (media_id.type == content::DesktopMediaID::Type::TYPE_WEB_CONTENTS) {
    ProcessScreenShareRestriction(
        application_title,
        GetScreenShareConfidentialContentsInfoForWebContents(
            GetWebContentsFromMediaId(media_id)),
        std::move(callback));
    return;
  }
  chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::Dlp>()) {
    LOG(WARNING) << "DLP mojo service not available";
    std::move(callback).Run(true);
    return;
  }

  int dlp_mojo_version =
      lacros_service->GetInterfaceVersion(crosapi::mojom::Dlp::Uuid_);
  if (dlp_mojo_version < int{crosapi::mojom::Dlp::MethodMinVersions::
                                 kCheckScreenShareRestrictionMinVersion}) {
    LOG(WARNING) << "DLP mojo service version does not support screen share "
                    "restrictions";
    std::move(callback).Run(true);
    return;
  }

  lacros_service->GetRemote<crosapi::mojom::Dlp>()->CheckScreenShareRestriction(
      ConvertToScreenShareArea(media_id), application_title,
      std::move(callback));
}

void DlpContentManagerLacros::OnScreenShareStarted(
    const std::string& label,
    std::vector<content::DesktopMediaID> screen_share_ids,
    const std::u16string& application_title,
    base::RepeatingClosure stop_callback,
    content::MediaStreamUI::StateChangeCallback state_change_callback,
    content::MediaStreamUI::SourceCallback source_callback) {
  for (const content::DesktopMediaID& media_id : screen_share_ids) {
    if (media_id.type == content::DesktopMediaID::Type::TYPE_WEB_CONTENTS) {
      AddOrUpdateScreenShare(label, media_id, application_title, stop_callback,
                             state_change_callback, source_callback);
    } else {
      chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
      auto delegate = std::make_unique<ScreenShareStateChangeDelegate>(
          label, media_id, state_change_callback, stop_callback);
      if (lacros_service->IsAvailable<crosapi::mojom::Dlp>()) {
        lacros_service->GetRemote<crosapi::mojom::Dlp>()->OnScreenShareStarted(
            label, ConvertToScreenShareArea(media_id), application_title,
            delegate->BindDelegate());
        running_remote_screen_shares_.push_back(std::move(delegate));
      }
    }
  }
  CheckRunningScreenShares();
}

void DlpContentManagerLacros::OnScreenShareStopped(
    const std::string& label,
    const content::DesktopMediaID& media_id) {
  if (media_id.type == content::DesktopMediaID::Type::TYPE_WEB_CONTENTS) {
    RemoveScreenShare(label, media_id);
  } else {
    chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
    if (lacros_service->IsAvailable<crosapi::mojom::Dlp>()) {
      lacros_service->GetRemote<crosapi::mojom::Dlp>()->OnScreenShareStopped(
          label, ConvertToScreenShareArea(media_id));
    }
    base::EraseIf(
        running_remote_screen_shares_,
        [=](const std::unique_ptr<
            DlpContentManagerLacros::ScreenShareStateChangeDelegate>& delegate)
            -> bool {
          return delegate->label() == label && delegate->media_id() == media_id;
        });
  }
}

void DlpContentManagerLacros::TabLocationMaybeChanged(
    content::WebContents* web_contents) {
  UpdateRestrictions(web_contents->GetNativeView());
}

DlpContentManagerLacros::ScreenShareStateChangeDelegate::
    ScreenShareStateChangeDelegate(
        const std::string& label,
        const content::DesktopMediaID& media_id,
        content::MediaStreamUI::StateChangeCallback state_change_callback,
        base::OnceClosure stop_callback)
    : label_(label),
      media_id_(media_id),
      state_change_callback_(std::move(state_change_callback)),
      stop_callback_(std::move(stop_callback)) {}

DlpContentManagerLacros::ScreenShareStateChangeDelegate::
    ~ScreenShareStateChangeDelegate() = default;

bool DlpContentManagerLacros::ScreenShareStateChangeDelegate::operator==(
    const DlpContentManagerLacros::ScreenShareStateChangeDelegate& other)
    const {
  return label_ == other.label_ && media_id_ == other.media_id_;
}

bool DlpContentManagerLacros::ScreenShareStateChangeDelegate::operator!=(
    const DlpContentManagerLacros::ScreenShareStateChangeDelegate& other)
    const {
  return !(*this == other);
}

mojo::PendingRemote<crosapi::mojom::StateChangeDelegate>
DlpContentManagerLacros::ScreenShareStateChangeDelegate::BindDelegate() {
  return receiver_.BindNewPipeAndPassRemoteWithVersion();
}

void DlpContentManagerLacros::ScreenShareStateChangeDelegate::OnPause() {
  state_change_callback_.Run(media_id_,
                             blink::mojom::MediaStreamStateChange::PAUSE);
}

void DlpContentManagerLacros::ScreenShareStateChangeDelegate::OnResume() {
  state_change_callback_.Run(media_id_,
                             blink::mojom::MediaStreamStateChange::PLAY);
}

void DlpContentManagerLacros::ScreenShareStateChangeDelegate::OnStop() {
  DCHECK(stop_callback_);
  if (stop_callback_) {
    std::move(stop_callback_).Run();
  }
}

DlpContentManagerLacros::DlpContentManagerLacros() = default;

DlpContentManagerLacros::~DlpContentManagerLacros() {
  // Clean up still observed windows.
  for (const auto& window_pair : window_webcontents_) {
    window_pair.first->RemoveObserver(this);
  }
}

void DlpContentManagerLacros::OnConfidentialityChanged(
    content::WebContents* web_contents,
    const DlpContentRestrictionSet& restriction_set) {
  DlpContentManager::OnConfidentialityChanged(web_contents, restriction_set);
  aura::Window* window = web_contents->GetNativeView();
  if (!window_webcontents_.contains(window)) {
    window_webcontents_[window] = {};
    window->AddObserver(this);
  }
  window_webcontents_[window].insert(web_contents);
  UpdateRestrictions(window);
  CheckRunningScreenShares();
}

void DlpContentManagerLacros::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  DlpContentManager::OnWebContentsDestroyed(web_contents);
  aura::Window* window = web_contents->GetNativeView();
  if (window_webcontents_.contains(window)) {
    window_webcontents_[window].erase(web_contents);
    UpdateRestrictions(window);
  }
}

void DlpContentManagerLacros::OnVisibilityChanged(
    content::WebContents* web_contents) {
  aura::Window* window = web_contents->GetNativeView();
  UpdateRestrictions(window);
}

void DlpContentManagerLacros::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  window_webcontents_.erase(window);
  confidential_windows_.erase(window);
}

void DlpContentManagerLacros::UpdateRestrictions(aura::Window* window) {
  DlpContentRestrictionSet new_restrictions;
  for (auto* web_contents : window_webcontents_[window]) {
    if (web_contents->GetNativeView()->IsVisible()) {
      new_restrictions.UnionWith(confidential_web_contents_[web_contents]);
    }
  }
  if (new_restrictions != confidential_windows_[window]) {
    confidential_windows_[window] = new_restrictions;
    chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
    if (lacros_service->IsAvailable<crosapi::mojom::Dlp>()) {
      lacros_service->GetRemote<crosapi::mojom::Dlp>()->DlpRestrictionsUpdated(
          lacros_window_utility::GetRootWindowUniqueId(window),
          ConvertRestrictionSetToMojo(new_restrictions));
    }
  }
}

DlpContentManager::ConfidentialContentsInfo
DlpContentManagerLacros::GetScreenShareConfidentialContentsInfo(
    const content::DesktopMediaID& media_id,
    content::WebContents* web_contents) const {
  if (media_id.type == content::DesktopMediaID::Type::TYPE_WEB_CONTENTS) {
    return GetScreenShareConfidentialContentsInfoForWebContents(web_contents);
  }
  NOTREACHED();
  return ConfidentialContentsInfo();
}

}  // namespace policy
