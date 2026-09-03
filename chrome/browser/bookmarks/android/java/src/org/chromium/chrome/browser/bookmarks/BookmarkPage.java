// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;

import java.util.function.Supplier;

/** A native page holding a {@link BookmarkManagerCoordinator} on _tablet_. */
@NullMarked
public class BookmarkPage extends BasicNativePage {
    private final BookmarkManagerCoordinator mBookmarkManagerCoordinator;
    private final BookmarkOpener mBookmarkOpener;
    private final String mTitle;

    // Nullable after destruction of BookmarkPage.
    private @Nullable BookmarkUiPrefs mBookmarkUiPrefs;

    /**
     * Create a new instance of the bookmarks page.
     *
     * @param windowAndroid The current {@link WindowAndroid} showing the bookmark UI.
     * @param activity The current {@link Activity} used to obtain resources or inflate views.
     * @param snackbarManager Allows control over the app snackbar.
     * @param bottomSheetControllerSupplier Supplier of the controller used to interact with the
     *     bottom sheet.
     * @param activityResultTracker Tracker of activity results.
     * @param profile The Profile associated with the bookmark UI.
     * @param host A NativePageHost to load urls.
     * @param bookmarkOpener Used to open bookmarks.
     * @param bookmarkManagerOpener Used to open bookmark manager.
     * @param priceDropNotificationManager Manager for price drop notifications.
     * @param signinAndHistorySyncActivityLauncher Launcher for signin and history sync activities.
     * @param deviceLockActivityLauncher Launcher for device lock activities.
     * @param backPressManager BackPressManager for processing back press events.
     */
    public BookmarkPage(
            WindowAndroid windowAndroid,
            Activity activity,
            SnackbarManager snackbarManager,
            Supplier<BottomSheetController> bottomSheetControllerSupplier,
            ActivityResultTracker activityResultTracker,
            Profile profile,
            NativePageHost host,
            BookmarkOpener bookmarkOpener,
            BookmarkManagerOpener bookmarkManagerOpener,
            PriceDropNotificationManager priceDropNotificationManager,
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            BackPressManager backPressManager) {
        super(host);

        mTitle = host.getContext().getString(R.string.bookmarks);
        mBookmarkOpener = bookmarkOpener;
        mBookmarkUiPrefs = new BookmarkUiPrefs(ChromeSharedPreferences.getInstance());
        // Provide the BackPressManager to the coordinator so it can manage itself.
        // The logic in the coordinator ensures that there is only one NATIVE_PAGE handler set
        // at a time.
        mBookmarkManagerCoordinator =
                new BookmarkManagerCoordinator(
                        windowAndroid,
                        activity,
                        false,
                        snackbarManager,
                        bottomSheetControllerSupplier,
                        activityResultTracker,
                        profile,
                        mBookmarkUiPrefs,
                        mBookmarkOpener,
                        bookmarkManagerOpener,
                        priceDropNotificationManager,
                        host::createEdgeToEdgePadAdjuster,
                        backPressManager,
                        signinAndHistorySyncActivityLauncher,
                        deviceLockActivityLauncher);

        mBookmarkManagerCoordinator.setBasicNativePage(this);
        initWithView(mBookmarkManagerCoordinator.getView());

        setBackPressHandler(mBookmarkManagerCoordinator, backPressManager);
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public String getHost() {
        return UrlConstants.BOOKMARKS_HOST;
    }

    @Override
    public void updateForUrl(String url) {
        super.updateForUrl(url);
        mBookmarkManagerCoordinator.updateForUrl(url);
    }

    @Override
    public boolean supportsEdgeToEdge() {
        return true;
    }

    @Override
    public void destroy() {
        super.destroy();
        mBookmarkManagerCoordinator.onDestroyed();

        if (mBookmarkUiPrefs != null) {
            mBookmarkUiPrefs.destroy();
            mBookmarkUiPrefs = null;
        }
    }

    public BookmarkManagerCoordinator getManagerForTesting() {
        return mBookmarkManagerCoordinator;
    }
}
