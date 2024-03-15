// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARE_PAGE_INFO_SHARING_BRIDGE_H_
#define CHROME_BROWSER_SHARE_PAGE_INFO_SHARING_BRIDGE_H_

#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"

namespace sharing {

// True if the profile identity supports page info sharing.
bool DoesProfileSupportPageInfo(signin::IdentityManager* identity_manager);

// True if the web contents of the tab supports page info sharing.
bool DoesWebContentsSupportPageInfo(content::WebContents* web_contents);

}  // namespace sharing

#endif  // CHROME_BROWSER_SHARE_PAGE_INFO_SHARING_BRIDGE_H_
