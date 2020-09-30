// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_service.h"

#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/sharesheet/share_action.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "ui/display/types/display_constants.h"
#include "ui/views/view.h"

namespace sharesheet {

SharesheetService::SharesheetService(Profile* profile)
    : profile_(profile),
      sharesheet_action_cache_(std::make_unique<SharesheetActionCache>()),
      app_service_proxy_(
          apps::AppServiceProxyFactory::GetForProfile(profile_)) {}

SharesheetService::~SharesheetService() = default;

void SharesheetService::ShowBubble(content::WebContents* web_contents,
                                   apps::mojom::IntentPtr intent,
                                   sharesheet::CloseCallback close_callback) {
  ShowBubble(web_contents, std::move(intent),
             /*contains_hosted_document=*/false, std::move(close_callback));
}

void SharesheetService::ShowBubble(content::WebContents* web_contents,
                                   apps::mojom::IntentPtr intent,
                                   bool contains_hosted_document,
                                   sharesheet::CloseCallback close_callback) {
  DCHECK(intent->action == apps_util::kIntentActionSend ||
         intent->action == apps_util::kIntentActionSendMultiple);
  auto sharesheet_service_delegate =
      std::make_unique<SharesheetServiceDelegate>(delegate_counter_++,
                                                  web_contents, this);
  ShowBubbleWithDelegate(std::move(sharesheet_service_delegate),
                         std::move(intent), contains_hosted_document,
                         std::move(close_callback));
}

// Cleanup delegate when bubble closes.
void SharesheetService::OnBubbleClosed(uint32_t id,
                                       const base::string16& active_action) {
  auto iter = active_delegates_.begin();
  while (iter != active_delegates_.end()) {
    if ((*iter)->GetId() == id) {
      if (!active_action.empty()) {
        ShareAction* share_action =
            sharesheet_action_cache_->GetActionFromName(active_action);
        if (share_action != nullptr)
          share_action->OnClosing(iter->get());
      }
      active_delegates_.erase(iter);
      break;
    }
    ++iter;
  }
}

void SharesheetService::OnTargetSelected(uint32_t delegate_id,
                                         const base::string16& target_name,
                                         const TargetType type,
                                         apps::mojom::IntentPtr intent,
                                         views::View* share_action_view) {
  SharesheetServiceDelegate* delegate = GetDelegate(delegate_id);
  if (delegate == nullptr)
    return;

  if (type == TargetType::kAction) {
    ShareAction* share_action =
        sharesheet_action_cache_->GetActionFromName(target_name);
    if (share_action == nullptr)
      return;
    delegate->OnActionLaunched();
    share_action->LaunchAction(delegate, share_action_view, std::move(intent));
  } else if (type == TargetType::kApp) {
    // TODO(crbug.com/1097623) Update this when we support more app types.
    sharesheet::SharesheetMetrics::RecordSharesheetActionMetrics(
        sharesheet::SharesheetMetrics::UserAction::kArc);
    auto launch_source = apps::mojom::LaunchSource::kFromSharesheet;
    app_service_proxy_->LaunchAppWithIntent(
        base::UTF16ToUTF8(target_name),
        apps::GetEventFlags(
            apps::mojom::LaunchContainer::kLaunchContainerWindow,
            WindowOpenDisposition::NEW_WINDOW,
            /*prefer_container=*/true),
        std::move(intent), launch_source, display::kDefaultDisplayId);
    delegate->CloseSharesheet();
  }
}

SharesheetServiceDelegate* SharesheetService::GetDelegate(
    uint32_t delegate_id) {
  auto iter = active_delegates_.begin();
  while (iter != active_delegates_.end()) {
    if ((*iter)->GetId() == delegate_id) {
      return iter->get();
    }
    ++iter;
  }
  return nullptr;
}

bool SharesheetService::HasShareTargets(const apps::mojom::IntentPtr& intent,
                                        bool contains_hosted_document) {
  std::vector<apps::IntentLaunchInfo> intent_launch_info =
      app_service_proxy_->GetAppsForIntent(intent);

  return sharesheet_action_cache_->HasVisibleActions(
             intent, contains_hosted_document) ||
         (!contains_hosted_document && !intent_launch_info.empty());
}

Profile* SharesheetService::GetProfile() {
  return profile_;
}

void SharesheetService::LoadAppIcons(
    std::vector<apps::IntentLaunchInfo> intent_launch_info,
    std::vector<TargetInfo> targets,
    size_t index,
    base::OnceCallback<void(std::vector<TargetInfo> targets)> callback) {
  if (index >= intent_launch_info.size()) {
    std::move(callback).Run(std::move(targets));
    return;
  }

  // Making a copy because we move |intent_launch_info| out below.
  auto app_id = intent_launch_info[index].app_id;
  auto app_type = app_service_proxy_->AppRegistryCache().GetAppType(app_id);
  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;
  constexpr bool allow_placeholder_icon = false;
  app_service_proxy_->LoadIcon(
      app_type, app_id, icon_type, kIconSize, allow_placeholder_icon,
      base::BindOnce(&SharesheetService::OnIconLoaded,
                     weak_factory_.GetWeakPtr(), std::move(intent_launch_info),
                     std::move(targets), index, std::move(callback)));
}

void SharesheetService::OnIconLoaded(
    std::vector<apps::IntentLaunchInfo> intent_launch_info,
    std::vector<TargetInfo> targets,
    size_t index,
    base::OnceCallback<void(std::vector<TargetInfo> targets)> callback,
    apps::mojom::IconValuePtr icon_value) {
  const auto& launch_entry = intent_launch_info[index];
  app_service_proxy_->AppRegistryCache().ForOneApp(
      launch_entry.app_id,
      [&launch_entry, &targets, &icon_value](const apps::AppUpdate& update) {
        targets.emplace_back(TargetType::kApp, icon_value->uncompressed,
                             base::UTF8ToUTF16(launch_entry.app_id),
                             base::UTF8ToUTF16(update.Name()),
                             base::UTF8ToUTF16(launch_entry.activity_label),
                             launch_entry.activity_name);
      });

  LoadAppIcons(std::move(intent_launch_info), std::move(targets), index + 1,
               std::move(callback));
}

void SharesheetService::OnAppIconsLoaded(
    std::unique_ptr<SharesheetServiceDelegate> delegate,
    apps::mojom::IntentPtr intent,
    sharesheet::CloseCallback close_callback,
    std::vector<TargetInfo> targets) {
  delegate->ShowBubble(std::move(targets), std::move(intent),
                       std::move(close_callback));
  active_delegates_.push_back(std::move(delegate));
}

void SharesheetService::ShowBubbleWithDelegate(
    std::unique_ptr<SharesheetServiceDelegate> delegate,
    apps::mojom::IntentPtr intent,
    bool contains_hosted_document,
    sharesheet::CloseCallback close_callback) {
  std::vector<TargetInfo> targets;
  auto& actions = sharesheet_action_cache_->GetShareActions();
  auto iter = actions.begin();
  while (iter != actions.end()) {
    if ((*iter)->ShouldShowAction(intent, contains_hosted_document)) {
      targets.emplace_back(TargetType::kAction, (*iter)->GetActionIcon(),
                           (*iter)->GetActionName(), (*iter)->GetActionName(),
                           base::nullopt, base::nullopt);
    }
    ++iter;
  }

  std::vector<apps::IntentLaunchInfo> intent_launch_info =
      contains_hosted_document ? std::vector<apps::IntentLaunchInfo>()
                               : app_service_proxy_->GetAppsForIntent(intent);
  sharesheet::SharesheetMetrics::RecordSharesheetAppCount(
      intent_launch_info.size());
  LoadAppIcons(std::move(intent_launch_info), std::move(targets), 0,
               base::BindOnce(&SharesheetService::OnAppIconsLoaded,
                              weak_factory_.GetWeakPtr(), std::move(delegate),
                              std::move(intent), std::move(close_callback)));
}

}  // namespace sharesheet
