// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_tab_helper.h"

#include "chrome/browser/profiles/profile.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension_messages.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#endif

SessionTabHelper::SessionTabHelper(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      session_id_(SessionID::NewUnique()),
      window_id_(SessionID::InvalidValue()) {}

SessionTabHelper::~SessionTabHelper() {
}

void SessionTabHelper::SetWindowID(const SessionID& id) {
  window_id_ = id;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Extension code in the renderer holds the ID of the window that hosts it.
  // Notify it that the window ID changed.
  web_contents()->SendToAllFrames(
      new ExtensionMsg_UpdateBrowserWindowId(MSG_ROUTING_NONE, id.id()));
#endif
}

// static
SessionID SessionTabHelper::IdForTab(const content::WebContents* tab) {
  const SessionTabHelper* session_tab_helper =
      tab ? SessionTabHelper::FromWebContents(tab) : NULL;
  return session_tab_helper ? session_tab_helper->session_id()
                            : SessionID::InvalidValue();
}

// static
SessionID SessionTabHelper::IdForWindowContainingTab(
    const content::WebContents* tab) {
  const SessionTabHelper* session_tab_helper =
      tab ? SessionTabHelper::FromWebContents(tab) : NULL;
  return session_tab_helper ? session_tab_helper->window_id()
                            : SessionID::InvalidValue();
}

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
void SessionTabHelper::UserAgentOverrideSet(const std::string& user_agent) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  SessionService* session = SessionServiceFactory::GetForProfile(profile);
  if (session)
    session->SetTabUserAgentOverride(window_id(), session_id(), user_agent);
}

void SessionTabHelper::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  SessionService* session_service = SessionServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  if (!session_service)
    return;

  int current_entry_index =
      web_contents()->GetController().GetCurrentEntryIndex();
  session_service->SetSelectedNavigationIndex(window_id(), session_id(),
                                              current_entry_index);
  const sessions::SerializedNavigationEntry navigation =
      sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
          current_entry_index,
          web_contents()->GetController().GetEntryAtIndex(current_entry_index));
  session_service->UpdateTabNavigation(window_id(), session_id(), navigation);
}

void SessionTabHelper::NavigationListPruned(
    const content::PrunedDetails& pruned_details) {
  SessionService* session_service = SessionServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  if (!session_service)
    return;

  session_service->TabNavigationPathPruned(
      window_id(), session_id(), pruned_details.index, pruned_details.count);
}

void SessionTabHelper::NavigationEntriesDeleted() {
  SessionService* session_service = SessionServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  if (!session_service)
    return;

  session_service->TabNavigationPathEntriesDeleted(window_id(), session_id());
}

void SessionTabHelper::NavigationEntryChanged(
    const content::EntryChangedDetails& change_details) {
  SessionService* session_service = SessionServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  if (!session_service)
    return;

  const sessions::SerializedNavigationEntry navigation =
      sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
          change_details.index, change_details.changed_entry);
  session_service->UpdateTabNavigation(window_id(), session_id(), navigation);
}
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
void SessionTabHelper::SetTabExtensionAppID(
    const std::string& extension_app_id) {
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  SessionService* session_service = SessionServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  if (!session_service)
    return;

  session_service->SetTabExtensionAppID(window_id(), session_id(),
                                        extension_app_id);
#endif
}
#endif

WEB_CONTENTS_USER_DATA_KEY_IMPL(SessionTabHelper)
