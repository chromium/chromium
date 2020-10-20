// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TAB_UTIL_H_
#define CHROME_BROWSER_TAB_CONTENTS_TAB_UTIL_H_

#include "content/public/browser/site_instance.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}

namespace tab_util {

// Helper to find the WebContents that originated the given request. Can be
// NULL if the tab has been closed or some other error occurs.
// Should only be called from the UI thread, since it accesses WebContents.
content::WebContents* GetWebContentsByID(int render_process_id,
                                         int render_view_id);

// Returns a new SiteInstance for WebUI and app URLs. Returns NULL otherwise.
scoped_refptr<content::SiteInstance> GetSiteInstanceForNewTab(Profile* profile,
                                                              GURL url);

}  // namespace tab_util

#endif  // CHROME_BROWSER_TAB_CONTENTS_TAB_UTIL_H_
