// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service_lookup.h"

#include "chrome/browser/buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service_base.h"
#include "chrome/browser/sessions/session_service_factory.h"

#if BUILDFLAG(ENABLE_APP_SESSION_SERVICE)
#include "chrome/browser/sessions/app_session_service_factory.h"
#endif

bool IsRelevantToAppSessionService(Browser::Type type) {
#if !BUILDFLAG(ENABLE_APP_SESSION_SERVICE)
  NOTREACHED();
  // IsRelevantToAppSessionService should never be called unless the buildflag
  // is enabled.
#endif
  return (type == Browser::Type::TYPE_APP ||
          type == Browser::Type::TYPE_APP_POPUP);
}

bool IsRelevantToAppSessionService(content::WebContents* web_contents) {
  return IsRelevantToAppSessionService(
      SessionServiceBase::GetBrowserTypeFromWebContents(web_contents));
}

bool IsRelevantToAppSessionService(Browser* browser) {
  return IsRelevantToAppSessionService(browser->type());
}

SessionServiceBase* GetAppropriateSessionServiceForProfile(
    const Browser* browser) {
#if BUILDFLAG(ENABLE_APP_SESSION_SERVICE)
  if (IsRelevantToAppSessionService(browser->type()))
    return AppSessionServiceFactory::GetForProfile(browser->profile());
#endif

  return SessionServiceFactory::GetForProfile(browser->profile());
}

SessionServiceBase* GetAppropriateSessionServiceForSessionRestore(
    const Browser* browser) {
#if BUILDFLAG(ENABLE_APP_SESSION_SERVICE)
  if (IsRelevantToAppSessionService(browser->type()))
    return AppSessionServiceFactory::GetForProfileForSessionRestore(
        browser->profile());
#endif

  return SessionServiceFactory::GetForProfileForSessionRestore(
      browser->profile());
}

SessionServiceBase* GetAppropriateSessionServiceIfExisting(
    const Browser* browser) {
#if BUILDFLAG(ENABLE_APP_SESSION_SERVICE)
  if (IsRelevantToAppSessionService(browser->type()))
    return AppSessionServiceFactory::GetForProfileIfExisting(
        browser->profile());
#endif

  return SessionServiceFactory::GetForProfileIfExisting(browser->profile());
}
