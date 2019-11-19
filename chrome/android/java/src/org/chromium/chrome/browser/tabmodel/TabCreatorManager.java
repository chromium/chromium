// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.Nullable;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

/**
 * An interface to return a {@link TabCreator} either for regular or incognito tabs.
 */
public interface TabCreatorManager {
    /**
     * Creates Tabs.  If the TabCreator creates Tabs asynchronously, null pointers will be returned
     * everywhere instead of a Tab.
     *
     * TODO(dfalcantara): Hunt down more places where we don't actually need to return a Tab.
     */
    public abstract class TabCreator {
        /**
         * @return Whether the TabCreator creates Tabs asynchronously.
         */
        public abstract boolean createsTabsAsynchronously();

        /**
         * Creates a new tab and posts to UI.
         * @param loadUrlParams parameters of the url load.
         * @param type Information about how the tab was launched.
         * @param parent the parent tab, if present.
         * @return The new tab or null if no tab was created.
         */
        @Nullable
        public abstract Tab createNewTab(
                LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent);

        /**
         * On restore, allows us to create a frozen version of a tab using saved tab state we read
         * from disk.
         * @param state    The tab state that the tab can be restored from.
         * @param id       The id to give the new tab.
         * @param index    The index for where to place the tab.
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
        @Nullable
        public abstract Tab launchUrl(String url, @TabLaunchType int type);

        /**
         * Creates a Tab to host the given WebContents.
         * @param parent      The parent tab, if present.
         * @param webContents The web contents to create a tab around.
         * @param type        The TabLaunchType describing how this tab was created.
         * @param url         URL to show in the Tab. (Needed only for asynchronous tab creation.)
         * @return            Whether a Tab was created successfully.
         */
        public abstract boolean createTabWithWebContents(
                @Nullable Tab parent, WebContents webContents, @TabLaunchType int type, String url);

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

        /**
         * Creates a new tab and loads the NTP.
         */
        public final void launchNTP() {
            try {
                TraceEvent.begin("TabCreator.launchNTP");
                launchUrl(UrlConstants.NTP_URL, TabLaunchType.FROM_CHROME_UI);
            } finally {
                TraceEvent.end("TabCreator.launchNTP");
            }
        }
    }

    /**
     * @return A {@link TabCreator} that will create either regular or incognito tabs.
     * @param incognito True if the method should return the TabCreator for incognito tabs, false
     *                  for regular tabs.
     */
    TabCreator getTabCreator(boolean incognito);
}
