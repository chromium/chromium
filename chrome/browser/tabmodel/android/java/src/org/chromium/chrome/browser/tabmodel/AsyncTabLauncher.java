// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;

public interface AsyncTabLauncher {
    /**
     * Creates and launches a new tab with the given LoadUrlParams.
     *
     * @param loadUrlParams parameters of the load.
     * @param type Information about how the tab was launched.
     * @param parent the parent tab, if present.
     */
    void launchNewTab(LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent);
}
