// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.app.Activity;
import android.net.Uri;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.embedder_support.util.UrlConstants;

/** Native page for managing browsing history. */
public class HistoryPage extends BasicNativePage {
    private HistoryManager mHistoryManager;
    private String mTitle;

    /**
     * Create a new instance of the history page.
     *
     * @param activity The {@link Activity} used to get context and instantiate the {@link
     *     HistoryManager}.
     * @param host A NativePageHost to load URLs.
     * @param snackbarManager The {@link SnackbarManager} used to display snackbars.
     * @param profile The Profile of the current tab.
     * @param bottomSheetController {@link BottomSheetController} object.
     * @param tabSupplier Supplies the current tab, null if the history UI will be shown in a
     *     separate activity.
     * @param url The URL used to address the HistoryPage.
     */
    public HistoryPage(
            Activity activity,
            NativePageHost host,
            SnackbarManager snackbarManager,
            Profile profile,
            BottomSheetController bottomSheetController,
            Supplier<Tab> tabSupplier,
            String url) {
        super(host);

        Uri uri = Uri.parse(url);
        assert uri.getHost().equals(UrlConstants.HISTORY_HOST);

        mHistoryManager =
                new HistoryManager(
                        activity,
                        /* isSeparateActivity= */ false,
                        snackbarManager,
                        profile,
                        () -> bottomSheetController,
                        tabSupplier,
                        new BrowsingHistoryBridge(profile.getOriginalProfile()),
                        new HistoryUmaRecorder(),
                        null,
                        /* shouldShowClearData= */ true,
                        /* launchedForApp= */ false,
                        /* showAppFilter= */ true);
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

    public HistoryManager getHistoryManagerForTesting() {
        return mHistoryManager;
    }
}
