// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.app.Activity;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.merchant_viewer.PageInfoStoreInfoController.StoreInfoActionHandler;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.PageInfoController.OpenedFromSource;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Helper class showing page info dialog for Clank. */
public class ChromePageInfo {
    private final @NonNull Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final @Nullable String mPublisher;
    private final @OpenedFromSource int mSource;
    private final @Nullable Supplier<StoreInfoActionHandler> mStoreInfoActionHandlerSupplier;
    private final @Nullable Supplier<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier;
    private final @Nullable TabCreator mTabCreator;

    /**
     * @param modalDialogManagerSupplier Supplier of modal dialog manager.
     * @param publisher The name of the publisher of the content.
     * @param source the source that triggered the popup.
     * @param storeInfoActionHandlerSupplier Supplier of {@link StoreInfoActionHandler}.
     * @param ephemeralTabCoordinatorSupplier Supplier of {@link EphemeralTabCoordinator}.
     * @param tabCreator {@link TabCreator} to handle a new tab creation.
     */
    public ChromePageInfo(
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier,
            @Nullable String publisher,
            @OpenedFromSource int source,
            @Nullable Supplier<StoreInfoActionHandler> storeInfoActionHandlerSupplier,
            @Nullable Supplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            @Nullable TabCreator tabCreator) {
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mPublisher = publisher;
        mSource = source;
        mStoreInfoActionHandlerSupplier = storeInfoActionHandlerSupplier;
        mEphemeralTabCoordinatorSupplier = ephemeralTabCoordinatorSupplier;
        mTabCreator = tabCreator;
    }

    /**
     * Show page info dialog.
     * @param tab Tab object containing the page whose information to be displayed.
     * @param pageInfoHighlight Providing the highlight row info related to this dialog.
     */
    public void show(Tab tab, ChromePageInfoHighlight pageInfoHighlight) {
        WebContents webContents = tab.getWebContents();
        if (webContents == null || !ProfileManager.isInitialized()) return;

        Activity activity = TabUtils.getActivity(tab);
        PageInfoController.show(
                activity,
                webContents,
                mPublisher,
                mSource,
                new ChromePageInfoControllerDelegate(
                        activity,
                        webContents,
                        mModalDialogManagerSupplier,
                        new OfflinePageUtils.TabOfflinePageLoadUrlDelegate(tab),
                        mStoreInfoActionHandlerSupplier,
                        mEphemeralTabCoordinatorSupplier,
                        pageInfoHighlight,
                        mTabCreator),
                pageInfoHighlight);
    }
}
