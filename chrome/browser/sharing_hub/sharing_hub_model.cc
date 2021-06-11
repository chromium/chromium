// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing_hub/sharing_hub_model.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"

namespace sharing_hub {

SharingHubModel::SharingHubModel(content::BrowserContext* context)
    : context_(context) {
  PopulateFirstPartyActions();
  PopulateThirdPartyActions();
}

SharingHubModel::~SharingHubModel() = default;

void SharingHubModel::GetActionList(content::WebContents* web_contents,
                                    std::vector<SharingHubAction>* list) {
  for (const auto& action : action_list_) {
    if (action.command_id == IDC_SEND_TAB_TO_SELF) {
      if (DoShowSendTabToSelfForWebContents(web_contents)) {
        list->push_back(action);
      }
    } else {
      list->push_back(action);
    }
  }
}

void SharingHubModel::PopulateFirstPartyActions() {
  action_list_.push_back(
      {IDC_COPY_URL, IDS_SHARING_HUB_COPY_LINK_LABEL, kCopyIcon, true});

  action_list_.push_back({IDC_SEND_TAB_TO_SELF,
                          IDS_CONTEXT_MENU_SEND_TAB_TO_SELF, kSendTabToSelfIcon,
                          true});

  action_list_.push_back({IDC_QRCODE_GENERATOR,
                          IDS_OMNIBOX_QRCODE_GENERATOR_ICON_LABEL,
                          kQrcodeGeneratorIcon, true});

  if (media_router::MediaRouterEnabled(context_)) {
    action_list_.push_back({IDC_ROUTE_MEDIA, IDS_SHARING_HUB_MEDIA_ROUTER_LABEL,
                            vector_icons::kMediaRouterIdleIcon, true});
  }

  action_list_.push_back(
      {IDC_SAVE_PAGE, IDS_SHARING_HUB_SAVE_PAGE_LABEL, kSavePageIcon, true});
}

void SharingHubModel::PopulateThirdPartyActions() {
  // TODO(1186833): add third party actions
}

bool SharingHubModel::DoShowSendTabToSelfForWebContents(
    content::WebContents* web_contents) {
  return send_tab_to_self::ShouldOfferFeature(web_contents);
}

}  // namespace sharing_hub
