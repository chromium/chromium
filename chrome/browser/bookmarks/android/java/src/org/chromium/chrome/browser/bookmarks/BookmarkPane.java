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
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;

import java.util.function.DoubleConsumer;
import java.util.function.Supplier;

/** A {@link Pane} representing history. */
@NullMarked
public class BookmarkPane extends PaneBase {

    // Below are dependencies of the pane itself.
    private final WindowAndroid mWindowAndroid;
    private final Activity mActivity;
    private final SnackbarManager mSnackbarManager;
    private final Supplier<BottomSheetController> mBottomSheetControllerSupplier;
    private final ActivityResultTracker mActivityResultTracker;
    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;

    private @Nullable BookmarkManagerCoordinator mBookmarkManager;
    private @Nullable BookmarkOpener mBookmarkOpener;

    /**
     * Create a new instance of the bookmarks pane.
     *
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     * @param windowAndroid The current {@link WindowAndroid} showing the bookmark UI.
     * @param activity Used as a dependency to BookmarkManager.
     * @param snackbarManager Used as a dependency to BookmarkManager.
     * @param bottomSheetControllerSupplier Supplier of the controller used to interact with the
     *     bottom sheet.
     * @param activityResultTracker Tracker of activity results.
     * @param profileProviderSupplier Used as a dependency to BookmarkManager.
     */
    public BookmarkPane(
            DoubleConsumer onToolbarAlphaChange,
            WindowAndroid windowAndroid,
            Activity activity,
            SnackbarManager snackbarManager,
            Supplier<BottomSheetController> bottomSheetControllerSupplier,
            ActivityResultTracker activityResultTracker,
            OneshotSupplier<ProfileProvider> profileProviderSupplier) {
        super(PaneId.BOOKMARKS, activity, onToolbarAlphaChange);
        mReferenceButtonDataSupplier.set(
                new ResourceButtonData(
                        R.string.menu_bookmarks, R.string.menu_bookmarks, R.drawable.ic_star_24dp));

        mWindowAndroid = windowAndroid;
        mActivity = activity;
        mSnackbarManager = snackbarManager;
        mProfileProviderSupplier = profileProviderSupplier;
        mBottomSheetControllerSupplier = bottomSheetControllerSupplier;
        mActivityResultTracker = activityResultTracker;
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
                            mWindowAndroid,
                            mActivity,
                            /* isDialogUi= */ false,
                            mSnackbarManager,
                            mBottomSheetControllerSupplier,
                            mActivityResultTracker,
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
