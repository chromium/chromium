// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/send_tab_to_self_desktop_util.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/desktop_notification_handler.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

namespace send_tab_to_self {

void CreateNewEntry(content::WebContents* tab,
                    const std::string& target_device_name,
                    const std::string& target_device_guid,
                    const GURL& link_url) {
  DCHECK(tab);

  GURL shared_url = link_url;
  std::string title;
  base::Time navigation_time = base::Time();

  content::NavigationEntry* navigation_entry =
      tab->GetController().GetLastCommittedEntry();

  // This should either be a valid link share or a valid tab share.
  DCHECK(link_url.is_valid() || navigation_entry);

  if (!link_url.is_valid()) {
    // This is not link share, get the values from the last navigation entry.
    shared_url = navigation_entry->GetURL();
    title = base::UTF16ToUTF8(navigation_entry->GetTitle());
    navigation_time = navigation_entry->GetTimestamp();
  }

  Profile* profile = Profile::FromBrowserContext(tab->GetBrowserContext());
  DCHECK(profile);
  SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();
  DCHECK(model);

  if (!model->IsReady()) {
    // TODO(https://crbug.com/1280681): Is this legit? In STTSv2, there may not
    // *be* a DesktopNotificationHandler for profile, and we're violating the
    // lifetime rules of DesktopNotificationHandler here I think.
    DesktopNotificationHandler(profile).DisplayFailureMessage(shared_url);
    return;
  }

  model->AddEntry(shared_url, title, navigation_time, target_device_guid);

  SendTabToSelfBubbleController* controller = send_tab_to_self::
      SendTabToSelfBubbleController::CreateOrGetFromWebContents(tab);
  controller->ShowConfirmationMessage();
}

}  // namespace send_tab_to_self
