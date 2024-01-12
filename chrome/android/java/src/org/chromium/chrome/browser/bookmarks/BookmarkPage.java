// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.ComponentName;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.embedder_support.util.UrlConstants;

/** A native page holding a {@link BookmarkManagerCoordinator} on _tablet_. */
public class BookmarkPage extends BasicNativePage {
    private BookmarkManagerCoordinator mBookmarkManagerCoordinator;
    private String mTitle;

    /**
     * Create a new instance of the bookmarks page.
     *
     * @param componentName The current activity component, used to open bookmarks.
     * @param snackbarManager Allows control over the app snackbar.
     * @param profile The Profile associated with the bookmark UI.
     * @param host A NativePageHost to load urls.
     */
    public BookmarkPage(
            ComponentName componentName,
            SnackbarManager snackbarManager,
            Profile profile,
            NativePageHost host) {
        super(host);

        mBookmarkManagerCoordinator =
                new BookmarkManagerCoordinator(
                        host.getContext(),
                        componentName,
                        false,
                        snackbarManager,
                        profile,
                        new BookmarkUiPrefs(ChromeSharedPreferences.getInstance()));
        mBookmarkManagerCoordinator.setBasicNativePage(this);
        mTitle = host.getContext().getResources().getString(R.string.bookmarks);

        initWithView(mBookmarkManagerCoordinator.getView());
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
    public void destroy() {
        mBookmarkManagerCoordinator.onDestroyed();
        mBookmarkManagerCoordinator = null;
        super.destroy();
    }

    public BookmarkManagerCoordinator getManagerForTesting() {
        return mBookmarkManagerCoordinator;
    }
}
