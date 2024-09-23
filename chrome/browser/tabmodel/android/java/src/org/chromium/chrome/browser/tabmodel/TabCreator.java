// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/**
 * Creates Tabs.  If the TabCreator creates Tabs asynchronously, null pointers will be returned
 * everywhere instead of a Tab.
 *
 * TODO(dfalcantara): Hunt down more places where we don't actually need to return a Tab.
 */
public abstract class TabCreator {

    /**
     * Creates a new tab and posts to UI.
     * @param loadUrlParams parameters of the url load.
     * @param type Information about how the tab was launched.
     * @param parent the parent tab, if present.
     * @return The new tab or null if no tab was created.
     */
    public abstract @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent);

    /**
     * Creates a new tab and posts to UI.
     *
     * @param loadUrlParams parameters of the url load.
     * @param type Information about how the tab was launched.
     * @param parent the parent tab, if present.
     * @param position the requested position (index in the tab model)
     * @return The new tab or null if no tab was created.
     */
    public abstract @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent, int position);

    /**
     * Creates a new tab and posts to UI.
     *
     * @param loadUrlParams parameters of the url load.
     * @param title The title to use for a lazily loaded tab.
     * @param type Information about how the tab was launched.
     * @param parent the parent tab, if present.
     * @param position the requested position (index in the tab model)
     * @return The new tab or null if no tab was created.
     */
    public abstract @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams,
            String title,
            @TabLaunchType int type,
            Tab parent,
            int position);

    /**
     * On restore, allows us to create a frozen version of a tab using saved tab state we read from
     * disk.
     *
     * @param state The tab state that the tab can be restored from.
     * @param id The id to give the new tab.
     * @param index The index for where to place the tab.
     */
    public abstract Tab createFrozenTab(TabState state, int id, int index);

    /**
     * Creates a new tab and loads the specified URL in it. This is a convenience method for
     * {@link #createNewTab} with the default {@link LoadUrlParams} and no parent tab.
     *
     * @param url the URL to open.
     * @param type the type of action that triggered that launch. Determines how the tab is
     *             opened (for example, in the foreground or background).
     * @return The new tab or null if no tab was created.
     */
    public abstract @Nullable Tab launchUrl(String url, @TabLaunchType int type);

    /**
     * Creates a Tab to host the given WebContents.
     * @param parent      The parent tab, if present.
     * @param webContents The web contents to create a tab around.
     * @param type        The TabLaunchType describing how this tab was created.
     * @param url         URL to show in the Tab. (Needed only for asynchronous tab creation.)
     * @return            Whether a Tab was created successfully.
     */
    public abstract boolean createTabWithWebContents(
            @Nullable Tab parent,
            WebContents webContents,
            @TabLaunchType int type,
            @NonNull GURL url);

    /**
     * Creates a tab around the native web contents pointer.
     * @param parent      The parent tab, if present.
     * @param webContents The web contents to create a tab around.
     * @param type        The TabLaunchType describing how this tab was created.
     * @return            Whether a Tab was created successfully.
     */
    public final boolean createTabWithWebContents(
            Tab parent, WebContents webContents, @TabLaunchType int type) {
        return createTabWithWebContents(parent, webContents, type, webContents.getVisibleUrl());
    }

    /** Creates a new tab and loads the NTP. */
    public final void launchNtp() {
        launchNtp(TabLaunchType.FROM_CHROME_UI);
    }

    /** Creates a new tab and loads the NTP. */
    public final void launchNtp(@TabLaunchType int type) {
        try {
            TraceEvent.begin("TabCreator.launchNtp");
            launchUrl(UrlConstants.NTP_URL, type);
        } finally {
            TraceEvent.end("TabCreator.launchNtp");
        }
    }
}
