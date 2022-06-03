// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.UpdateDelta;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;

/**
 * The central class responsible for showing the download progress UI. Implemented by both the info
 * bar and messages UI versions. Tracks updates for download items, offline items, android
 * downloads etc. and computes the current state of the UI to be shown.
 */
public interface DownloadMessageUiController extends OfflineContentProvider.Observer {
    /**
     * Shows the message that download has started. Unlike other methods in this class, this
     * method doesn't require an {@link OfflineItem} and is invoked by the backend to provide a
     * responsive feedback to the users even before the download has actually started.
     */
    void onDownloadStarted();

    /** Associates a notification ID with the tracked download for future usage. */
    // TODO(shaktisahu): Find an alternative way after moving to offline content provider.
    void onNotificationShown(ContentId id, int notificationId);

    /** OfflineContentProvider.Observer methods. */
    @Override
    void onItemsAdded(List<OfflineItem> items);

    @Override
    void onItemRemoved(ContentId id);

    @Override
    void onItemUpdated(OfflineItem item, UpdateDelta updateDelta);

    /** @return Whether the UI is currently showing. */
    boolean isShowing();

    /** Called when activity is launched or configuration is changed. */
    default void onConfigurationChanged(Context context,
            Supplier<MessageDispatcher> messageDispatcher, ModalDialogManager modalDialogManager,
            ActivityTabProvider activityTabProvider) {}
}
