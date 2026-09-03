// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneBase;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.ui.actions.button.ResourceButtonData;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;

import java.util.function.DoubleConsumer;
import java.util.function.Function;
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
    private final Function<Profile, BookmarkOpener> mBookmarkOpenerFactory;
    private final BookmarkManagerOpener mBookmarkManagerOpener;
    private final Function<Profile, PriceDropNotificationManager>
            mPriceDropNotificationManagerFactory;
    private final SigninAndHistorySyncActivityLauncher mSigninAndHistorySyncActivityLauncher;
    private final DeviceLockActivityLauncher mDeviceLockActivityLauncher;

    private @Nullable BookmarkManagerCoordinator mBookmarkManager;
    private @Nullable BookmarkOpener mBookmarkOpener;
    private @Nullable BookmarkUiPrefs mBookmarkUiPrefs;

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
     * @param bookmarkOpenerFactory Factory to create BookmarkOpener for a profile.
     * @param bookmarkManagerOpener Used to open the bookmark manager.
     * @param priceDropNotificationManagerFactory Factory to create PriceDropNotificationManager.
     * @param signinAndHistorySyncActivityLauncher Launcher for signin and history sync activities.
     * @param deviceLockActivityLauncher Launcher for device lock activities.
     */
    public BookmarkPane(
            DoubleConsumer onToolbarAlphaChange,
            WindowAndroid windowAndroid,
            Activity activity,
            SnackbarManager snackbarManager,
            Supplier<BottomSheetController> bottomSheetControllerSupplier,
            ActivityResultTracker activityResultTracker,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            Function<Profile, BookmarkOpener> bookmarkOpenerFactory,
            BookmarkManagerOpener bookmarkManagerOpener,
            Function<Profile, PriceDropNotificationManager> priceDropNotificationManagerFactory,
            SigninAndHistorySyncActivityLauncher signinAndHistorySyncActivityLauncher,
            DeviceLockActivityLauncher deviceLockActivityLauncher) {
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
        mBookmarkOpenerFactory = bookmarkOpenerFactory;
        mBookmarkManagerOpener = bookmarkManagerOpener;
        mPriceDropNotificationManagerFactory = priceDropNotificationManagerFactory;
        mSigninAndHistorySyncActivityLauncher = signinAndHistorySyncActivityLauncher;
        mDeviceLockActivityLauncher = deviceLockActivityLauncher;
    }

    @Override
    public void destroy() {
        destroyManagerAndRemoveView();
    }

    @Override
    public void notifyLoadHint(@LoadHint int loadHint) {
        if (loadHint == LoadHint.HOT && mBookmarkManager == null) {
            Profile originalProfile =
                    assumeNonNull(mProfileProviderSupplier.get()).getOriginalProfile();
            mBookmarkOpener = mBookmarkOpenerFactory.apply(originalProfile);
            mBookmarkUiPrefs = new BookmarkUiPrefs(ChromeSharedPreferences.getInstance());
            mBookmarkManager =
                    new BookmarkManagerCoordinator(
                            mWindowAndroid,
                            mActivity,
                            /* isDialogUi= */ false,
                            mSnackbarManager,
                            mBottomSheetControllerSupplier,
                            mActivityResultTracker,
                            originalProfile,
                            mBookmarkUiPrefs,
                            mBookmarkOpener,
                            mBookmarkManagerOpener,
                            mPriceDropNotificationManagerFactory.apply(originalProfile),
                            // TODO(crbug.com/427776544): make bookmark pane support edge to edge.
                            /* edgeToEdgePadAdjusterGenerator= */ null,
                            /* backPressManager= */ null,
                            mSigninAndHistorySyncActivityLauncher,
                            mDeviceLockActivityLauncher);
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
            mBookmarkManager.onDestroyed();
            mBookmarkManager = null;
        }

        if (mBookmarkOpener != null) {
            mBookmarkOpener = null;
        }

        if (mBookmarkUiPrefs != null) {
            mBookmarkUiPrefs.destroy();
            mBookmarkUiPrefs = null;
        }
        mRootView.removeAllViews();
    }
}
