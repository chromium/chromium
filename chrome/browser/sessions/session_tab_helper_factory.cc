// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/session_tab_helper.h"

#include "chrome/browser/buildflags.h"
#include "chrome/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_lookup.h"
#include "content/public/browser/web_contents.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_web_contents_observer.h"
#endif

namespace {

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
sessions::SessionTabHelperDelegate* GetSessionTabHelperDelegate(
    content::WebContents* web_contents) {
  // With AppSessionService, we now need to know if the WebContents
  // belongs to an AppSessionService or SessionService.
  if (IsRelevantToAppSessionService(web_contents)) {
    return AppSessionServiceFactory::GetForProfile(
        Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  }
  return SessionServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
}
#endif

}  // namespace

void CreateSessionServiceTabHelper(content::WebContents* contents) {
  if (sessions::SessionTabHelper::FromWebContents(contents))
    return;

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  sessions::SessionTabHelper::DelegateLookup lookup =
      base::BindRepeating(&GetSessionTabHelperDelegate);
#else
  sessions::SessionTabHelper::DelegateLookup lookup;
#endif
  sessions::SessionTabHelper::CreateForWebContents(contents, std::move(lookup));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  auto* observer =
      extensions::ExtensionWebContentsObserver::GetForWebContents(contents);

  if (observer) {
    observer->ListenToWindowIdChangesFrom(
        sessions::SessionTabHelper::FromWebContents(contents));
  }
#endif
}
