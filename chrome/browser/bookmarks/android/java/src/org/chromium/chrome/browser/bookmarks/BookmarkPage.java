// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.ComponentName;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.embedder_support.util.UrlConstants;

/** A native page holding a {@link BookmarkManagerCoordinator} on _tablet_. */
@NullMarked
public class BookmarkPage extends BasicNativePage {
    private final BookmarkManagerCoordinator mBookmarkManagerCoordinator;
    private final BookmarkOpener mBookmarkOpener;
    private final String mTitle;

    /**
     * Create a new instance of the bookmarks page.
     *
     * @param componentName The current activity component, used to open bookmarks.
     * @param snackbarManager Allows control over the app snackbar.
     * @param profile The Profile associated with the bookmark UI.
     * @param host A NativePageHost to load urls.
     */
    public BookmarkPage(
            SnackbarManager snackbarManager,
            Profile profile,
            NativePageHost host,
            @Nullable ComponentName componentName,
            BackPressManager backPressManager) {
        super(host);

        mTitle = host.getContext().getString(R.string.bookmarks);

        mBookmarkOpener =
                new BookmarkOpenerImpl(
                        () -> BookmarkModel.getForProfile(profile),
                        /* context= */ host.getContext(),
                        componentName);

        // Provide the BackPressManager to the coordinator so it can manage itself.
        // The logic in the coordinator ensures that there is only one NATIVE_PAGE handler set
        // at a time.
        mBookmarkManagerCoordinator =
                new BookmarkManagerCoordinator(
                        host.getContext(),
                        false,
                        snackbarManager,
                        profile,
                        new BookmarkUiPrefs(ChromeSharedPreferences.getInstance()),
                        mBookmarkOpener,
                        new BookmarkManagerOpenerImpl(),
                        PriceDropNotificationManagerFactory.create(profile),
                        host::createEdgeToEdgePadAdjuster,
                        backPressManager);

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
    }

    public BookmarkManagerCoordinator getManagerForTesting() {
        return mBookmarkManagerCoordinator;
    }
}
