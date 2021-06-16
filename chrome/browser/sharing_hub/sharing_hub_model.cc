// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing_hub/sharing_hub_model.h"

#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/share/core/share_targets.h"
#include "chrome/browser/share/proto/share_target.pb.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace sharing_hub {

SharingHubModel::SharingHubModel(content::BrowserContext* context)
    : context_(context) {
  PopulateFirstPartyActions();
  sharing::ShareTargets::GetInstance()->AddObserver(this);
}

SharingHubModel::~SharingHubModel() {
  sharing::ShareTargets::GetInstance()->RemoveObserver(this);
}

void SharingHubModel::GetFirstPartyActionList(
    content::WebContents* web_contents,
    std::vector<SharingHubAction>* list) {
  for (const auto& action : first_party_action_list_) {
    if (action.command_id == IDC_SEND_TAB_TO_SELF) {
      if (DoShowSendTabToSelfForWebContents(web_contents)) {
        list->push_back(action);
      }
    } else if (action.command_id == IDC_QRCODE_GENERATOR) {
      if (qrcode_generator::QRCodeGeneratorBubbleController::
              IsGeneratorAvailable(web_contents->GetLastCommittedURL())) {
        list->push_back(action);
      }
    } else {
      list->push_back(action);
    }
  }
}

void SharingHubModel::GetThirdPartyActionList(
    content::WebContents* web_contents,
    std::vector<SharingHubAction>* list) {
  for (const auto& action : third_party_action_list_) {
    list->push_back(action);
  }
}

void SharingHubModel::ExecuteThirdPartyAction(Profile* profile, int id) {
  auto url_it = third_party_action_urls_.find(id);
  if (url_it == third_party_action_urls_.end())
    return;

  NavigateParams params(profile, url_it->second, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.tabstrip_add_types = TabStripModel::ADD_ACTIVE;
  Navigate(&params);
}

void SharingHubModel::PopulateFirstPartyActions() {
  first_party_action_list_.push_back(
      {IDC_COPY_URL, l10n_util::GetStringUTF16(IDS_SHARING_HUB_COPY_LINK_LABEL),
       kCopyIcon, true});

  first_party_action_list_.push_back(
      {IDC_SEND_TAB_TO_SELF,
       l10n_util::GetStringUTF16(IDS_CONTEXT_MENU_SEND_TAB_TO_SELF),
       kSendTabToSelfIcon, true});

  first_party_action_list_.push_back(
      {IDC_QRCODE_GENERATOR,
       l10n_util::GetStringUTF16(IDS_OMNIBOX_QRCODE_GENERATOR_ICON_LABEL),
       kQrcodeGeneratorIcon, true});

  if (media_router::MediaRouterEnabled(context_)) {
    first_party_action_list_.push_back(
        {IDC_ROUTE_MEDIA,
         l10n_util::GetStringUTF16(IDS_SHARING_HUB_MEDIA_ROUTER_LABEL),
         vector_icons::kMediaRouterIdleIcon, true});
  }

  first_party_action_list_.push_back(
      {IDC_SAVE_PAGE,
       l10n_util::GetStringUTF16(IDS_SHARING_HUB_SAVE_PAGE_LABEL),
       kSavePageIcon, true});
}

void SharingHubModel::PopulateThirdPartyActions() {
  // Note: The third party action id must be greater than 0, otherwise the
  // action will be disabled in the app menu.
  std::string locale = "GLOBAL";
  int id = 1;
  if (third_party_targets_) {
    for (const sharing::mojom::ShareTarget& target :
         third_party_targets_->targets()) {
      // get ICON from target.icon();
      third_party_action_list_.push_back({id,
                                          base::ASCIIToUTF16(target.nickname()),
                                          kQrcodeGeneratorIcon, false});
      third_party_action_urls_[id] = GURL(target.url());
    }
  }
}

bool SharingHubModel::DoShowSendTabToSelfForWebContents(
    content::WebContents* web_contents) {
  return send_tab_to_self::ShouldOfferFeature(web_contents);
}

void SharingHubModel::OnShareTargetsUpdated(
    std::unique_ptr<sharing::mojom::ShareTargets> share_target) {
  third_party_targets_ = std::move(share_target);
  PopulateThirdPartyActions();
}

}  // namespace sharing_hub
