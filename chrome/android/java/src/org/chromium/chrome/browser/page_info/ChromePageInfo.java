// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.app.Activity;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.merchant_viewer.PageInfoStoreInfoController.StoreInfoActionHandler;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.PageInfoController.OpenedFromSource;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Helper class showing page info dialog for Clank.
 */
public class ChromePageInfo {
    private final @NonNull Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final @Nullable String mPublisher;
    private final @OpenedFromSource int mSource;
    private final @Nullable Supplier<StoreInfoActionHandler> mStoreInfoActionHandlerSupplier;

    /**
     * @param modalDialogManagerSupplier Supplier of modal dialog manager.
     * @param publisher The name of the publisher of the content.
     * @param source the source that triggered the popup.
     * @param storeInfoActionHandlerSupplier Supplier of {@link StoreInfoActionHandler}.
     */
    public ChromePageInfo(@NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier,
            @Nullable String publisher, @OpenedFromSource int source,
            @Nullable Supplier<StoreInfoActionHandler> storeInfoActionHandlerSupplier) {
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mPublisher = publisher;
        mSource = source;
        mStoreInfoActionHandlerSupplier = storeInfoActionHandlerSupplier;
    }

    /**
     * Show page info dialog.
     * @param tab Tab object containing the page whose information to be displayed.
     * @param permission The {@link ContentSettingsType} to be highlighted on this page.
     * @param fromStoreIcon Whether user enters page info via the store icon in omnibox, used to
     *         determine whether to highlight the "Store info" row.
     */
    public void show(Tab tab, @ContentSettingsType int permission, boolean fromStoreIcon) {
        WebContents webContents = tab.getWebContents();
        Activity activity = TabUtils.getActivity(tab);
        PageInfoController.show(activity, webContents, mPublisher, mSource,
                new ChromePageInfoControllerDelegate(activity, webContents,
                        mModalDialogManagerSupplier,
                        new OfflinePageUtils.TabOfflinePageLoadUrlDelegate(tab),
                        mStoreInfoActionHandlerSupplier, fromStoreIcon),
                permission);
    }
}
