// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mahi/mahi_tab_helper.h"

#include <string_view>

#include "chrome/browser/chromeos/mahi/mahi_web_contents_manager.h"
#include "chromeos/constants/chromeos_features.h"

namespace mahi {

// static
void MahiTabHelper::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (!chromeos::features::IsMahiEnabled()) {
    return;
  }
  MahiTabHelper::CreateForWebContents(web_contents);
}

MahiTabHelper::MahiTabHelper(content::WebContents* web_contents)
    : content::WebContentsUserData<MahiTabHelper>(*web_contents),
      content::WebContentsObserver(web_contents) {}

void MahiTabHelper::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  // Only fire an event if the web content has finished document loading.
  // Otherwise, it would be handled by
  // `DocumentOnLoadCompletedInPrimaryMainFrame`.
  if (web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
    MahiWebContentsManager::Get()->OnFocusedPageLoadComplete(web_contents());
  }
}

void MahiTabHelper::DocumentOnLoadCompletedInPrimaryMainFrame() {
  // Ignore the events from unfocused pages.
  if (!web_contents()->GetFocusedFrame()) {
    return;
  }
  MahiWebContentsManager::Get()->OnFocusedPageLoadComplete(web_contents());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MahiTabHelper);

}  // namespace mahi
