// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.app.Activity;
import android.net.Uri;

import org.chromium.base.supplier.SupplierUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.Supplier;

/** Native page for managing browsing history. */
@NullMarked
public class HistoryPage extends BasicNativePage {
    private HistoryManager mHistoryManager;
    private final String mTitle;

    /**
     * Create a new instance of the history page.
     *
     * @param profile The Profile of the current tab.
     * @param windowAndroid The current {@link WindowAndroid} showing the history UI.
     * @param activity The {@link Activity} used to get context and instantiate the {@link
     *     HistoryManager}.
     * @param host A NativePageHost to load URLs.
     * @param snackbarManager The {@link SnackbarManager} used to display snackbars.
     * @param bottomSheetController {@link BottomSheetController} object.
     * @param modalDialogManagerSupplier Supplies the {@link ModalDialogManager}.
     * @param activityResultTracker Tracker of activity results.
     * @param tabSupplier Supplies the current tab, null if the history UI will be shown in a
     *     separate activity.
     * @param url The URL used to address the HistoryPage.
     */
    public HistoryPage(
            Profile profile,
            WindowAndroid windowAndroid,
            Activity activity,
            NativePageHost host,
            SnackbarManager snackbarManager,
            BottomSheetController bottomSheetController,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            ActivityResultTracker activityResultTracker,
            Supplier<@Nullable Tab> tabSupplier,
            String url,
            BackPressManager backPressManager) {
        super(host);

        Uri uri = Uri.parse(url);
        assert UrlConstants.HISTORY_HOST.equals(uri.getHost());

        mHistoryManager =
                new HistoryManager(
                        profile,
                        windowAndroid,
                        activity,
                        /* isSeparateActivity= */ false,
                        snackbarManager,
                        SupplierUtils.of(bottomSheetController),
                        modalDialogManagerSupplier,
                        activityResultTracker,
                        tabSupplier,
                        new BrowsingHistoryBridge(profile.getOriginalProfile()),
                        new HistoryUmaRecorder(),
                        null,
                        /* shouldShowClearData= */ true,
                        /* launchedForApp= */ false,
                        /* showAppFilter= */ true,
                        /* openHistoryItemCallback= */ null,
                        host::createEdgeToEdgePadAdjuster);
        mTitle = host.getContext().getString(R.string.menu_history);

        initWithView(mHistoryManager.getView());

        setBackPressHandler(mHistoryManager, backPressManager);
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public String getHost() {
        return UrlConstants.HISTORY_HOST;
    }

    @SuppressWarnings("NullAway")
    @Override
    public void destroy() {
        mHistoryManager.onDestroyed();
        mHistoryManager = null;
        super.destroy();
    }

    public @Nullable HistoryManager getHistoryManagerForTesting() {
        return mHistoryManager;
    }
}
