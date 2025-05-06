// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/desktop/data_sharing_ui_delegate_desktop.h"

#include "chrome/browser/collaboration/collaboration_service_factory.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/data_sharing/collaboration_controller_delegate_desktop.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"
#include "components/collaboration/public/collaboration_service.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "url/gurl.h"

namespace data_sharing {

DataSharingUIDelegateDesktop::DataSharingUIDelegateDesktop(Profile* profile)
    : profile_(profile) {}

DataSharingUIDelegateDesktop::~DataSharingUIDelegateDesktop() = default;

void DataSharingUIDelegateDesktop::HandleShareURLIntercepted(
    const GURL& url,
    std::unique_ptr<ShareURLInterceptionContext> context) {
  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  if (browser) {
    auto delegate =
        std::make_unique<CollaborationControllerDelegateDesktop>(browser);
    collaboration::CollaborationServiceFactory::GetForProfile(profile_)
        ->StartJoinFlow(std::move(delegate), url);
  }
}

}  // namespace data_sharing
