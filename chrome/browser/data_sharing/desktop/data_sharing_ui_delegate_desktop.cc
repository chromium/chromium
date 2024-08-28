// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/desktop/data_sharing_ui_delegate_desktop.h"

#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "url/gurl.h"

namespace data_sharing {

DataSharingUIDelegateDesktop::DataSharingUIDelegateDesktop(Profile* profile)
    : profile_(profile) {}

DataSharingUIDelegateDesktop::~DataSharingUIDelegateDesktop() = default;

void DataSharingUIDelegateDesktop::HandleShareURLIntercepted(const GURL& url) {
  DataSharingService* const service =
      DataSharingServiceFactory::GetForProfile(profile_);
  const data_sharing::DataSharingService::ParseURLResult token =
      service->ParseDataSharingURL(url);
  if (!token->IsValid()) {
    return;
  }
  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  if (browser) {
    DataSharingBubbleController::GetOrCreateForBrowser(
        chrome::FindLastActiveWithProfile(profile_))
        ->Show(token.value());
  }
}

}  // namespace data_sharing
