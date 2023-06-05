// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_api_tab_helper.h"

#include "chrome/browser/storage_access_api/storage_access_api_service.h"
#include "chrome/browser/storage_access_api/storage_access_api_service_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

StorageAccessAPITabHelper::~StorageAccessAPITabHelper() = default;

void StorageAccessAPITabHelper::FrameReceivedUserActivation(
    content::RenderFrameHost* rfh) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(service_);

  if (rfh->IsInPrimaryMainFrame()) {
    // No need to do anything in a main frame.
    return;
  }

  service_->RenewPermissionGrant(
      rfh->GetLastCommittedOrigin(),
      rfh->GetParentOrOuterDocument()->GetLastCommittedOrigin());
}

StorageAccessAPITabHelper::StorageAccessAPITabHelper(
    content::WebContents* web_contents,
    StorageAccessAPIService* service)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<StorageAccessAPITabHelper>(*web_contents),
      service_(service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(service_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(StorageAccessAPITabHelper);
