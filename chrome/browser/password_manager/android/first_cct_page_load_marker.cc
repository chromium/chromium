// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/first_cct_page_load_marker.h"

#include "content/public/browser/web_contents.h"

// static
void FirstCctPageLoadMarker::CreateForWebContents(
    content::WebContents* web_contents) {
  // This should only be created once, when the CCT is initialized.
  if (web_contents->GetUserData(UserDataKey())) {
    return;
  }
  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new FirstCctPageLoadMarker(web_contents)));
}

// static
bool FirstCctPageLoadMarker::ConsumeMarker(content::WebContents* web_contents) {
  if (web_contents->GetUserData(UserDataKey())) {
    web_contents->RemoveUserData(UserDataKey());
    return true;
  }
  return false;
}

FirstCctPageLoadMarker::FirstCctPageLoadMarker(
    content::WebContents* web_contents)
    : content::WebContentsUserData<FirstCctPageLoadMarker>(*web_contents) {}

FirstCctPageLoadMarker::~FirstCctPageLoadMarker() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(FirstCctPageLoadMarker);
