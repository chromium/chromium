// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.creator;

import org.chromium.content_public.browser.LoadUrlParams;

/** Interface for opening URLS in a new tab.*/
public interface NewTabCreator {
    /**
     * Creates a new tab with the given params.
     * @param params the URL parameters needed to open the tab.
     */
    void createNewTab(LoadUrlParams params);
}
