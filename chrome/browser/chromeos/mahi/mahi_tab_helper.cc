// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mahi/mahi_tab_helper.h"

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/strings/string_piece.h"
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

// A tab should be skipped if it is empty, blank or default page.
bool MahiTabHelper::ShouldSkip() {
  static constexpr auto kSkipUrls = base::MakeFixedFlatSet<base::StringPiece>({
      // blank and default pages.
      "about:blank",
      "chrome://newtab/",
  });

  const std::string& url = web_contents()->GetURL().spec();
  return url.empty() || base::Contains(kSkipUrls, url);
}

void MahiTabHelper::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  if (ShouldSkip()) {
    return;
  }
  MahiWebContentsManager::Get()->OnFocusChanged(web_contents());

  // Only fire an event if the web content has finished document loading.
  // Otherwise, it would be handled by
  // `DocumentOnLoadCompletedInPrimaryMainFrame`.
  if (web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
    MahiWebContentsManager::Get()->OnFocusedPageLoadComplete(web_contents());
  }
}

void MahiTabHelper::DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (ShouldSkip()) {
    return;
  }
  // Ignore the events from unfocused pages.
  if (!web_contents()->GetFocusedFrame()) {
    return;
  }
  MahiWebContentsManager::Get()->OnFocusedPageLoadComplete(web_contents());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MahiTabHelper);

}  // namespace mahi
