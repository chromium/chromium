// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

/** Coordinates the desktop android bookmark popup. */
@NullMarked
public class BookmarkPopupCoordinator {
    private final BookmarkPopupMediator mMediator;
    private final BookmarkPopupView mView;
    private final PropertyModel mPropertyModel;
    private final AnchoredPopupWindow mPopupWindow;
    private final BookmarkImageFetcher mBookmarkImageFetcher;

    /**
     * Constructs the BookmarkPopupCoordinator. This represents the MVC component responsible for
     * managing the popup view shown when adding or editing a bookmark via the desktop window.
     *
     * @param activity The Android activity.
     * @param profile The current Profile.
     * @param anchor The anchor view for the popup window.
     * @param bookmarkManagerOpener Interface to open the bookmark manager.
     * @param shoppingService Shopping service to fetch price tracking info if available.
     * @param priceDropNotificationManager Manager to handle price drop notifications.
     */
    public BookmarkPopupCoordinator(
            Activity activity,
            Profile profile,
            View anchor,
            BookmarkManagerOpener bookmarkManagerOpener,
            @Nullable ShoppingService shoppingService,
            PriceDropNotificationManager priceDropNotificationManager) {
        mView =
                (BookmarkPopupView)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.bookmark_popup, /* root= */ null);

        mPropertyModel = new PropertyModel(BookmarkPopupProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mPropertyModel, mView, BookmarkPopupViewBinder::bind);

        int popupWidth =
                activity.getResources().getDimensionPixelSize(R.dimen.bookmark_popup_width);

        mPopupWindow =
                new AnchoredPopupWindow.Builder(
                                activity,
                                anchor,
                                AppCompatResources.getDrawable(activity, R.drawable.menu_bg_tinted),
                                () -> mView,
                                new ViewRectProvider(anchor))
                        .setOutsideTouchable(true)
                        .setFocusable(true)
                        .setMaxWidth(popupWidth)
                        .setDesiredContentWidth(popupWidth)
                        .setDismissOnScreenSizeChange(true)
                        .build();

        BookmarkModel bookmarkModel = BookmarkModel.getForProfile(profile);

        mBookmarkImageFetcher =
                new BookmarkImageFetcher(
                        profile,
                        activity,
                        bookmarkModel,
                        ImageFetcherFactory.createImageFetcher(
                                ImageFetcherConfig.DISK_CACHE_ONLY, profile.getProfileKey()),
                        BookmarkViewUtils.getRoundedIconGenerator(
                                activity, BookmarkRowDisplayPref.VISUAL));

        mMediator =
                new BookmarkPopupMediator(
                        mPropertyModel,
                        bookmarkModel,
                        bookmarkManagerOpener,
                        mBookmarkImageFetcher,
                        activity,
                        profile,
                        shoppingService,
                        priceDropNotificationManager,
                        () -> mPopupWindow.dismiss());
        mPopupWindow.addOnDismissListener(mMediator::destroy);
        mPopupWindow.addOnDismissListener(mView::destroy);
    }

    public void show(BookmarkId bookmarkId, boolean isNewBookmark) {
        mMediator.show(bookmarkId, isNewBookmark);
        mPopupWindow.show();
    }

    /** Destroys the coordinator, dismissing the popup. */
    public void destroy() {
        mPopupWindow.dismiss();
    }

    /** Returns the popup window for testing. */
    public AnchoredPopupWindow getPopupWindowForTesting() {
        return mPopupWindow;
    }
}
