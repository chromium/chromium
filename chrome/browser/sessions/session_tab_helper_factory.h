// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_TAB_HELPER_FACTORY_H_
#define CHROME_BROWSER_SESSIONS_SESSION_TAB_HELPER_FACTORY_H_

namespace content {
class WebContents;
}

void CreateSessionServiceTabHelper(content::WebContents* contents);

#endif  // CHROME_BROWSER_SESSIONS_SESSION_TAB_HELPER_FACTORY_H_
