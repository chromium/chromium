// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.document;

import android.app.Activity;
import android.content.Intent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.multiwindow.MultiInstanceOrchestrator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.AsyncTabLauncher;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Asynchronously creates tabs by creating/starting up activities. To explicitly launch a URL in a
 * new tab in a new or existing .Main activity window, also see {@link
 * MultiInstanceOrchestrator#openUrlInOtherWindow(Activity, LoadUrlParams, int, boolean, boolean)}.
 */
@NullMarked
public class ChromeAsyncTabLauncher implements AsyncTabLauncher {
    private final boolean mIsIncognito;

    /**
     * Creates a TabDelegate.
     * @param incognito Whether or not the TabDelegate handles the creation of incognito tabs.
     */
    public ChromeAsyncTabLauncher(boolean incognito) {
        mIsIncognito = incognito;
    }

    /**
     * Creates a new tab and loads the specified URL in it. This is a convenience method for {@link
     * #launchNewTab} with the default {@link LoadUrlParams} and no parent tab.
     *
     * @param url the URL to open.
     * @param type the type of action that triggered that launch. Determines how the tab is opened
     *     (for example, in the foreground or background).
     */
    public void launchUrl(String url, @TabLaunchType int type) {
        launchNewTab(new LoadUrlParams(url), type, null);
    }

    @Override
    public void launchNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, @Nullable Tab parent) {
        AsyncTabCreationParams asyncParams = new AsyncTabCreationParams(loadUrlParams);
        launchNewTab(asyncParams, type, parent == null ? Tab.INVALID_TAB_ID : parent.getId());
    }

    /**
     * Launches a Tab to host the given WebContents asynchronously.
     *
     * @param asyncParams Parameters to create the Tab with, including the URL.
     * @param type Information about how the tab was launched.
     * @param parentId ID of the parent tab, if it exists.
     */
    public void launchNewTab(
            AsyncTabCreationParams asyncParams, @TabLaunchType int type, int parentId) {
        assert asyncParams != null;

        // Tabs shouldn't be launched in affiliated mode when a webcontents exists.
        assert !(type == TabLaunchType.FROM_LONGPRESS_BACKGROUND
                && asyncParams.getWebContents() != null);

        Intent intent =
                IntentHandler.createAsyncNewTabIntent(asyncParams, parentId, type, mIsIncognito);
        IntentHandler.startActivityForTrustedIntent(intent);
    }

    /**
     * Passes the supplied web app launch intent to the IntentHandler.
     *
     * @param intent Web app launch intent.
     */
    public void launchNewStandaloneFrame(Intent intent) {
        assert intent != null;
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        IntentHandler.startActivityForTrustedIntent(intent);
    }
}
