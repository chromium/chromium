// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TAB_UTIL_H_
#define CHROME_BROWSER_TAB_CONTENTS_TAB_UTIL_H_

#include "content/public/browser/site_instance.h"
#include "url/gurl.h"

class Profile;

namespace tab_util {

// Returns a new SiteInstance for WebUI and app URLs. Returns NULL otherwise.
scoped_refptr<content::SiteInstance> GetSiteInstanceForNewTab(Profile* profile,
                                                              GURL url);

}  // namespace tab_util

#endif  // CHROME_BROWSER_TAB_CONTENTS_TAB_UTIL_H_
