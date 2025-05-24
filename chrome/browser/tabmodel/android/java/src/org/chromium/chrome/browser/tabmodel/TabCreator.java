// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/**
 * Creates Tabs. If the TabCreator creates Tabs asynchronously, null pointers will be returned
 * everywhere instead of a Tab.
 *
 * <p>TODO(dfalcantara): Hunt down more places where we don't actually need to return a Tab.
 */
@NullMarked
public abstract class TabCreator {

    /**
     * Creates a new tab and posts to UI.
     *
     * @param loadUrlParams parameters of the url load.
     * @param type Information about how the tab was launched.
     * @param parent the parent tab, if present.
     * @return The new tab or null if no tab was created.
     */
    public abstract @Nullable Tab createNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, @Nullable Tab parent);

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
            LoadUrlParams loadUrlParams,
            @TabLaunchType int type,
            @Nullable Tab parent,
            int position);

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
            @Nullable Tab parent,
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
     *
     * @param parent The parent Tab, if present.
     * @param webContents The web contents to create a Tab around.
     * @param type The TabLaunchType describing how this Tab was created.
     * @param url URL to show in the Tab. (Needed only for asynchronous tab creation.)
     * @param addTabToModel Whether the newly created Tab should be added to the tab model.
     *     Typically this should be true, however, sometimes it is beneficial to create a Tab
     *     without adding it to the current TabModel (e.g. if the Tab will ultimately be shown to
     *     the user in a new window).
     * @return The new Tab or null if a Tab was not created successfully.
     */
    public abstract Tab createTabWithWebContents(
            @Nullable Tab parent,
            WebContents webContents,
            @TabLaunchType int type,
            GURL url,
            boolean addTabToModel);

    /**
     * Creates a Tab to host the given WebContents and adds it to the TabModel.
     *
     * @param parent The parent Tab, if present.
     * @param webContents The web contents to create a Tab around.
     * @param type The TabLaunchType describing how this Tab was created.
     * @return The new Tab or null if a Tab was not created successfully.
     */
    public final Tab createTabWithWebContents(
            Tab parent, WebContents webContents, @TabLaunchType int type) {
        return createTabWithWebContents(parent, webContents, type, webContents.getVisibleUrl());
    }

    /**
     * Creates a Tab to host the given WebContents and adds it to the TabModel.
     *
     * @param parent The parent Tab, if present.
     * @param webContents The web contents to create a Tab around.
     * @param type The TabLaunchType describing how this Tab was created.
     * @param url URL to show in the Tab. (Needed only for asynchronous tab creation.)
     * @return The new Tab or null if a Tab was not created successfully.
     */
    public final Tab createTabWithWebContents(
            @Nullable Tab parent, WebContents webContents, @TabLaunchType int type, GURL url) {
        return createTabWithWebContents(parent, webContents, type, url, true);
    }

    /**
     * Creates a {@link Tab} with the same history stack as {@param parent}.
     *
     * @param parent The tab to copy.
     * @param type The {@code TabLaunchType} (should be {@code FROM_HISTORY_NAVIGATION_FOREGROUND}
     *     or {@code FROM_HISTORY_NAVIGATION_BACKGROUND}.
     * @return The {@link Tab} which was created.
     */
    public @Nullable abstract Tab createTabWithHistory(
            @Nullable Tab parent, @TabLaunchType int type);

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

    /** Semi-tag interface to denote dependency and provide a setter for {@link TabModel}. */
    public interface NeedsTabModel {
        void setTabModel(TabModel tabModel);
    }

    /**
     * Semi-tag interface to denote dependency and provide a setter for {@link
     * TabModelOrderController}.
     */
    public interface NeedsTabModelOrderController {
        void setTabModelOrderController(TabModelOrderController tabModelOrderController);
    }
}
