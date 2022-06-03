// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;
import android.content.Intent;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Creates {@link Tab}s asynchronously.
 */
public abstract class AsyncTabCreator extends TabCreator {
    /**
     * Creates a tab in the "other" window in multi-window mode. This will only work if
     * MultiWindowUtils#isOpenInOtherWindowSupported() is true for the given activity.
     *
     * @param loadUrlParams Parameters specifying the URL to load and other navigation details.
     * @param activity      The current {@link Activity}
     * @param parentId      The ID of the parent tab, or {@link Tab#INVALID_TAB_ID}.
     */
    public abstract void createTabInOtherWindow(
            LoadUrlParams loadUrlParams, Activity activity, int parentId);

    /**
     * Creates a Tab to host the given WebContents asynchronously.
     * @param asyncParams     Parameters to create the Tab with, including the URL.
     * @param type            Information about how the tab was launched.
     * @param parentId        ID of the parent tab, if it exists.
     */
    public abstract void createNewTab(
            AsyncTabCreationParams asyncParams, @TabLaunchType int type, int parentId);

    /**
     * Passes the supplied web app launch intent to the IntentHandler.
     * @param intent Web app launch intent.
     */
    public abstract void createNewStandaloneFrame(Intent intent);
}
