// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.download.DownloadInfoBarController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItemSchedule;

/**
 * An {@link InfoBar} to provide information about currently running downloads.
 */
public class DownloadProgressInfoBar {
    /**
     * Represents the client of this InfoBar. Provides hooks to take actions on various UI events
     * associated with the InfoBar.
     */
    public interface Client {
        /**
         * Called when a link is clicked by the user.
         * @param itemId The ContentId of the item currently being shown in the InfoBar.
         */
        void onLinkClicked(
                @Nullable ContentId itemId, @Nullable final OfflineItemSchedule schedule);

        /**
         * Called when the InfoBar is closed either implicitly or explicitly by the user.
         * @param explicitly Whether the InfoBar was closed explicitly by the user from close
         * button.
         */
        void onInfoBarClosed(boolean explicitly);
    }

    /**
     * @return The tab associated with this infobar.
     */
    public Tab getTab() {
        return null;
    }

    /**
     * Updates an existing {@link DownloadProgressInfoBar} with the new information.
     * @param info The information to be updated on the UI.
     */
    public void updateInfoBar(DownloadInfoBarController.DownloadProgressInfoBarData info) {}

    /**
     * Creates and shows the {@link DownloadProgressInfoBar}.
     * @param tab The tab that the {@link DownloadProgressInfoBar} should be shown in.
     */
    public static void createInfoBar(
            Client client, Tab tab, DownloadInfoBarController.DownloadProgressInfoBarData info) {}

    /**
     * Closes the {@link DownloadProgressInfoBar}.
     */
    public void closeInfoBar() {}
}
