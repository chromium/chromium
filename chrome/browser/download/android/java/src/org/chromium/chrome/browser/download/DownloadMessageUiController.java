// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.UpdateDelta;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.util.List;

/**
 * The central class responsible for showing the download progress UI. Implemented by both the info
 * bar and messages UI versions. Tracks updates for download items, offline items, android
 * downloads etc. and computes the current state of the UI to be shown.
 */
public interface DownloadMessageUiController extends OfflineContentProvider.Observer {
    /** A delegate to provide chrome layer dependencies. */
    interface Delegate {
        /** @return The context used for obtaining resources. */
        @Nullable
        Context getContext();

        /** @return The {@link MessageDispatcher} used for showing messages. */
        @Nullable
        MessageDispatcher getMessageDispatcher();

        /** @return The {@link ModalDialogManager} for showing download later dialog. */
        @Nullable
        ModalDialogManager getModalDialogManager();

        /**
         * Called by the controller before updating the UI so that it is only using the last focused
         * activity for all purposes.
         * @return True if we did a switch to another activity, false otherwise.
         */
        boolean maybeSwitchToFocusedActivity();

        /** Called to open the downloads page. */
        void openDownloadsPage(OTRProfileID otrProfileID, @DownloadOpenSource int source);

        /**
         * Called to open the download associated with the given {@link
         * contentId}.
         */
        void openDownload(
                ContentId contentId,
                OTRProfileID otrProfileID,
                @DownloadOpenSource int source,
                Context context);

        /** Called to remove a notification. */
        void removeNotification(int notificationId, DownloadInfo downloadInfo);
    }

    /**
     * Shows the message that download has started. Unlike other methods in this class, this
     * method doesn't require an {@link OfflineItem} and is invoked by the backend to provide a
     * responsive feedback to the users even before the download has actually started.
     */
    void onDownloadStarted();

    /** Associates a notification ID with the tracked download for future usage. */
    // TODO(shaktisahu): Find an alternative way after moving to offline content provider.
    void onNotificationShown(ContentId id, int notificationId);

    /**
     * Registers a new URL source for which a download interstitial download will be initiated.
     * @param originalUrl The URL of the download.
     */
    void addDownloadInterstitialSource(GURL originalUrl);

    /**
     * Returns true if the given download information matches an interstitial download.
     * @param originalUrl The URL of the download.
     * @param guid Unique GUID of the download.
     */
    boolean isDownloadInterstitialItem(GURL originalUrl, String guid);

    /** Shows a message that asks for the user confirmation before the actual download starts. */
    void showIncognitoDownloadMessage(Callback<Boolean> callback);

    /** OfflineContentProvider.Observer methods. */
    @Override
    void onItemsAdded(List<OfflineItem> items);

    @Override
    void onItemRemoved(ContentId id);

    @Override
    void onItemUpdated(OfflineItem item, UpdateDelta updateDelta);

    /** @return Whether the UI is currently showing. */
    boolean isShowing();
}
