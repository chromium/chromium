// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import org.chromium.content_public.browser.WebContents;

/** Interface to create a web contents*/
public interface WebContentsCreator {
    // create a web contents;
    WebContents createWebContents();
}
