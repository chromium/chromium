// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.embedder_support.util.UrlConstants;

/**
 * Native page for managing browsing history.
 */
public class HistoryPage extends BasicNativePage {
    private HistoryManager mHistoryManager;
    private String mTitle;

    /**
     * Create a new instance of the history page.
     * @param activity The {@link Activity} used to get context and instantiate the
     *                 {@link HistoryManager}.
     * @param host A NativePageHost to load URLs.
     * @param snackbarManager The {@link SnackbarManager} used to display snackbars.
     * @param isIncognito Whether the incognito tab model is currently selected.
     * @param tabCreatorManager Allows creation of tabs in different models, null if the history UI
     *                          will be shown in a separate activity.
     * @param tabSupplier Supplies the current tab, null if the history UI will be shown in a
     *                    separate activity.
     */
    public HistoryPage(Activity activity, NativePageHost host, SnackbarManager snackbarManager,
            boolean isIncognito, TabCreatorManager tabCreatorManager, Supplier<Tab> tabSupplier) {
        super(host);

        mHistoryManager = new HistoryManager(
                activity, false, snackbarManager, isIncognito, tabCreatorManager, tabSupplier);
        mTitle = host.getContext().getResources().getString(R.string.menu_history);

        initWithView(mHistoryManager.getView());
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public String getHost() {
        return UrlConstants.HISTORY_HOST;
    }

    @Override
    public void destroy() {
        mHistoryManager.onDestroyed();
        mHistoryManager = null;
        super.destroy();
    }

    @VisibleForTesting
    public HistoryManager getHistoryManagerForTesting() {
        return mHistoryManager;
    }
}
