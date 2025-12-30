// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.ComponentName;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneBase;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.ResourceButtonData;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;

import java.util.function.DoubleConsumer;

/** A {@link Pane} representing history. */
@NullMarked
public class BookmarkPane extends PaneBase {

    // Below are dependencies of the pane itself.
    private final SnackbarManager mSnackbarManager;
    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;

    private @Nullable BookmarkManagerCoordinator mBookmarkManager;
    private @Nullable BookmarkOpener mBookmarkOpener;

    /**
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     * @param activity Used as a dependency to BookmarkManager.
     * @param snackbarManager Used as a dependency to BookmarkManager.
     * @param profileProviderSupplier Used as a dependency to BookmarkManager.
     */
    public BookmarkPane(
            DoubleConsumer onToolbarAlphaChange,
            Activity activity,
            SnackbarManager snackbarManager,
            OneshotSupplier<ProfileProvider> profileProviderSupplier) {
        super(PaneId.BOOKMARKS, activity, onToolbarAlphaChange);
        mReferenceButtonDataSupplier.set(
                new ResourceButtonData(
                        R.string.menu_bookmarks, R.string.menu_bookmarks, R.drawable.ic_star_24dp));

        mSnackbarManager = snackbarManager;
        mProfileProviderSupplier = profileProviderSupplier;
    }

    @Override
    public void destroy() {
        destroyManagerAndRemoveView();
    }

    @Override
    public void notifyLoadHint(@LoadHint int loadHint) {
        if (loadHint == LoadHint.HOT && mBookmarkManager == null) {
            ComponentName componentName = ((Activity) mContext).getComponentName();
            Profile originalProfile =
                    assumeNonNull(mProfileProviderSupplier.get()).getOriginalProfile();
            mBookmarkOpener =
                    new BookmarkOpenerImpl(
                            () -> BookmarkModel.getForProfile(originalProfile),
                            mContext,
                            componentName);
            mBookmarkManager =
                    new BookmarkManagerCoordinator(
                            mContext,
                            /* isDialogUi= */ false,
                            mSnackbarManager,
                            originalProfile,
                            new BookmarkUiPrefs(ChromeSharedPreferences.getInstance()),
                            mBookmarkOpener,
                            new BookmarkManagerOpenerImpl(),
                            PriceDropNotificationManagerFactory.create(originalProfile),
                            // TODO(crbug.com/427776544): make bookmark pane support edge to edge.
                            /* edgeToEdgePadAdjusterGenerator= */ null,
                            /* backPressManager= */ null);
            UrlConstantResolver resolver =
                    UrlConstantResolverFactory.getForProfile(originalProfile);
            mBookmarkManager.updateForUrl(resolver.getBookmarksPageUrl());
            mRootView.addView(mBookmarkManager.getView());
        } else if (loadHint == LoadHint.COLD) {
            destroyManagerAndRemoveView();
        }
    }

    private void destroyManagerAndRemoveView() {
        if (mBookmarkManager != null) {
            mBookmarkOpener = null;
            mBookmarkManager.onDestroyed();
            mBookmarkManager = null;
        }
        mRootView.removeAllViews();
    }
}
