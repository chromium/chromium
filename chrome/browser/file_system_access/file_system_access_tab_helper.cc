// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/file_system_access_tab_helper.h"

#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "content/public/browser/navigation_handle.h"

FileSystemAccessTabHelper::~FileSystemAccessTabHelper() = default;

void FileSystemAccessTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation) {
  // We only care about top-level navigations that actually committed.
  if (!navigation->IsInMainFrame() || !navigation->HasCommitted())
    return;

  auto src_origin = url::Origin::Create(navigation->GetPreviousMainFrameURL());
  auto dest_origin = url::Origin::Create(navigation->GetURL());

  if (src_origin == dest_origin)
    return;

  // Navigated away from |src_origin|, tell permission context to check if
  // permissions need to be revoked.
  auto* context =
      FileSystemAccessPermissionContextFactory::GetForProfileIfExists(
          web_contents()->GetBrowserContext());
  if (context)
    context->NavigatedAwayFromOrigin(src_origin);
}

void FileSystemAccessTabHelper::WebContentsDestroyed() {
  auto src_origin = url::Origin::Create(web_contents()->GetLastCommittedURL());

  // Navigated away from |src_origin|, tell permission context to check if
  // permissions need to be revoked.
  auto* context =
      FileSystemAccessPermissionContextFactory::GetForProfileIfExists(
          web_contents()->GetBrowserContext());
  if (context)
    context->NavigatedAwayFromOrigin(src_origin);
}

FileSystemAccessTabHelper::FileSystemAccessTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FileSystemAccessTabHelper)
