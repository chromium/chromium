// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service_lookup.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/session_service_base.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

bool IsRelevantToAppSessionService(BrowserWindowInterface::Type type) {
  return (type == BrowserWindowInterface::Type::TYPE_APP ||
          type == BrowserWindowInterface::Type::TYPE_APP_POPUP);
}

bool IsRelevantToAppSessionService(content::WebContents* web_contents) {
  return IsRelevantToAppSessionService(
      SessionServiceBase::GetBrowserTypeFromWebContents(web_contents));
}

bool IsRelevantToAppSessionService(BrowserWindowInterface* browser) {
  return IsRelevantToAppSessionService(browser->GetType());
}

SessionServiceBase* GetAppropriateSessionServiceForProfile(
    BrowserWindowInterface* browser) {
  if (IsRelevantToAppSessionService(browser->GetType())) {
    return AppSessionServiceFactory::GetForProfile(browser->GetProfile());
  }

  return SessionServiceFactory::GetForProfile(browser->GetProfile());
}

SessionServiceBase* GetAppropriateSessionServiceForSessionRestore(
    BrowserWindowInterface* browser) {
  if (IsRelevantToAppSessionService(browser->GetType())) {
    return AppSessionServiceFactory::GetForProfileForSessionRestore(
        browser->GetProfile());
  }

  return SessionServiceFactory::GetForProfileForSessionRestore(
      browser->GetProfile());
}

SessionServiceBase* GetAppropriateSessionServiceIfExisting(
    BrowserWindowInterface* browser) {
  if (IsRelevantToAppSessionService(browser->GetType())) {
    return AppSessionServiceFactory::GetForProfileIfExisting(
        browser->GetProfile());
  }

  return SessionServiceFactory::GetForProfileIfExisting(browser->GetProfile());
}
