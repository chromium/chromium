// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager_lacros.h"

#include "chrome/browser/ui/lacros/window_utility.h"
#include "chromeos/crosapi/mojom/dlp.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

namespace policy {

namespace {
static DlpContentManagerLacros* g_dlp_content_manager = nullptr;

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
    case DlpRulesManager::Level::kNotSet:
      return crosapi::mojom::DlpRestrictionLevel::kAllow;
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

}  // namespace

// static
DlpContentManagerLacros* DlpContentManagerLacros::Get() {
  if (!g_dlp_content_manager) {
    g_dlp_content_manager = new DlpContentManagerLacros();
    g_dlp_content_manager->Init();
  }
  return g_dlp_content_manager;
}

void DlpContentManagerLacros::CheckScreenShareRestriction(
    const content::DesktopMediaID& media_id,
    const std::u16string& application_title,
    OnDlpRestrictionCheckedCallback callback) {
  if (media_id.type == content::DesktopMediaID::Type::TYPE_WEB_CONTENTS) {
    ProcessScreenShareRestriction(
        application_title,
        GetScreenShareConfidentialContentsInfoForWebContents(
            media_id.web_contents_id),
        std::move(callback));
  } else {
    // No restrictions apply.
    // TODO(crbug.com/1278814): Pass the check to Ash to process there.
    std::move(callback).Run(true);
  }
}

DlpContentManagerLacros::DlpContentManagerLacros() = default;
DlpContentManagerLacros::~DlpContentManagerLacros() = default;

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
    if (web_contents->GetVisibility() == content::Visibility::VISIBLE) {
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

}  // namespace policy
