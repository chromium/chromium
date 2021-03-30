// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import android.app.Activity;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
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

    /**
     * @param modalDialogManagerSupplier Supplier of modal dialog manager.
     * @param publisher The name of the publisher of the content.
     * @param source the source that triggered the popup.
     */
    public ChromePageInfo(@NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier,
            @Nullable String publisher, @OpenedFromSource int source) {
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mPublisher = publisher;
        mSource = source;
    }

    /**
     * Show page info dialog.
     * @param tab Tab object containing the page whose information to be displayed.
     * @param permission The {@link ContentSettingsType} to be highlighted on this page.
     */
    public void show(Tab tab, @ContentSettingsType int permission) {
        WebContents webContents = tab.getWebContents();
        Activity activity = TabUtils.getActivity(tab);
        PageInfoController.show(activity, webContents, mPublisher, mSource,
                new ChromePageInfoControllerDelegate(activity, webContents,
                        mModalDialogManagerSupplier,
                        new OfflinePageUtils.TabOfflinePageLoadUrlDelegate(tab)),
                new ChromePermissionParamsListBuilderDelegate(), permission);
    }
}
