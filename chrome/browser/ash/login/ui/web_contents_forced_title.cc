// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/web_contents_forced_title.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

namespace chromeos {

// static
void WebContentsForcedTitle::CreateForWebContentsWithTitle(
    content::WebContents* web_contents,
    const std::u16string& title) {
  if (FromWebContents(web_contents))
    return;

  web_contents->UpdateTitleForEntry(nullptr, title);
  web_contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new WebContentsForcedTitle(web_contents, title)));
}

WebContentsForcedTitle::WebContentsForcedTitle(
    content::WebContents* web_contents,
    const std::u16string& title)
    : content::WebContentsObserver(web_contents), title_(title) {}

WebContentsForcedTitle::~WebContentsForcedTitle() {}

void WebContentsForcedTitle::TitleWasSet(content::NavigationEntry* entry) {
  if (!entry || entry->GetTitle() != title_)
    web_contents()->UpdateTitleForEntry(entry, title_);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsForcedTitle)

}  // namespace chromeos
