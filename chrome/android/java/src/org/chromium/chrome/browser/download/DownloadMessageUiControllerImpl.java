// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;

import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.UpdateDelta;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.List;

/**
 * Message UI specific implementation of {@link DownloadMessageUiController}.
 */
public class DownloadMessageUiControllerImpl implements DownloadMessageUiController {
    /** Constructor. */
    public DownloadMessageUiControllerImpl(OTRProfileID otrProfileID, Context context,
            MessageDispatcher messageDispatcher, ModalDialogManager modalDialogManager) {}

    @Override
    public void onDownloadStarted() {}

    @Override
    public void onDownloadItemUpdated(DownloadItem downloadItem) {}

    @Override
    public void onDownloadItemRemoved(ContentId contentId) {}

    @Override
    public void onNotificationShown(ContentId id, int notificationId) {}

    @Override
    public void onItemsAdded(List<OfflineItem> items) {}

    @Override
    public void onItemRemoved(ContentId id) {}

    @Override
    public void onItemUpdated(OfflineItem item, UpdateDelta updateDelta) {}

    @Override
    public boolean isShowing() {
        return false;
    }
}
