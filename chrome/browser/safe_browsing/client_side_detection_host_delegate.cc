// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/client_side_detection_host_delegate.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/client_side_detection_service_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/user_interaction_observer.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/client_side_detection_host.h"
#include "components/safe_browsing/core/db/database_manager.h"

namespace safe_browsing {

// static
std::unique_ptr<ClientSideDetectionHost>
ClientSideDetectionHostDelegate::CreateHost(content::WebContents* tab) {
  return ClientSideDetectionHost::Create(
      tab, std::make_unique<ClientSideDetectionHostDelegate>(tab));
}

ClientSideDetectionHostDelegate::ClientSideDetectionHostDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}
ClientSideDetectionHostDelegate::~ClientSideDetectionHostDelegate() = default;

bool ClientSideDetectionHostDelegate::HasSafeBrowsingUserInteractionObserver() {
  return SafeBrowsingUserInteractionObserver::FromWebContents(web_contents_);
}

PrefService* ClientSideDetectionHostDelegate::GetPrefs() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return profile ? profile->GetPrefs() : nullptr;
}

scoped_refptr<SafeBrowsingDatabaseManager>
ClientSideDetectionHostDelegate::GetSafeBrowsingDBManager() {
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  return sb_service ? sb_service->database_manager().get() : nullptr;
}

scoped_refptr<BaseUIManager>
ClientSideDetectionHostDelegate::GetSafeBrowsingUIManager() {
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  return sb_service ? sb_service->ui_manager() : nullptr;
}

ClientSideDetectionService*
ClientSideDetectionHostDelegate::GetClientSideDetectionService() {
  return ClientSideDetectionServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
}

}  // namespace safe_browsing
