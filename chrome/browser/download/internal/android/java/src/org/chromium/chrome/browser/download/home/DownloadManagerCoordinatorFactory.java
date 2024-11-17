// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import android.app.Activity;
import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Factory class to build a DownloadManagerCoordinator instance. */
public class DownloadManagerCoordinatorFactory {
    private DownloadManagerCoordinatorFactory() {}

    /** Builds a {@link DownloadManagerCoordinatorImpl} instance. */
    public static DownloadManagerCoordinator create(
            Activity activity,
            DownloadManagerUiConfig config,
            Supplier<Boolean> exploreOfflineTabVisibilitySupplier,
            Callback<Context> settingsNavigation,
            SnackbarManager snackbarManager,
            ModalDialogManager modalDialogManager,
            Tracker tracker,
            FaviconProvider faviconProvider,
            OfflineContentProvider provider,
            DiscardableReferencePool discardableReferencePool) {
        return new DownloadManagerCoordinatorImpl(
                activity,
                config,
                exploreOfflineTabVisibilitySupplier,
                settingsNavigation,
                snackbarManager,
                modalDialogManager,
                tracker,
                faviconProvider,
                provider,
                discardableReferencePool);
    }
}
