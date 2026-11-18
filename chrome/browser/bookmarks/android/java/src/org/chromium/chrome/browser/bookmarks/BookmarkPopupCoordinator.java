// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.IntDef;
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

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Coordinates the desktop android bookmark popup. */
@NullMarked
public class BookmarkPopupCoordinator {
    /** The user intent that opened the popup. */
    @IntDef({
        BookmarkPopupLaunchType.BOOKMARK,
        BookmarkPopupLaunchType.PRICE_TRACKING_ENABLE,
        BookmarkPopupLaunchType.PRICE_TRACKING_MANAGE,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface BookmarkPopupLaunchType {
        /** A bookmark was added or is being edited. */
        int BOOKMARK = 0;

        /** Price tracking was requested for the bookmark, e.g. from the price tracking button. */
        int PRICE_TRACKING_ENABLE = 1;

        /** The bookmark is already price tracked, and the user wants to manage it. */
        int PRICE_TRACKING_MANAGE = 2;
    }

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
        if (activity.getResources().getDisplayMetrics().widthPixels < popupWidth) {
            mPropertyModel.set(BookmarkPopupProperties.IMAGE_VISIBLE, false);
            popupWidth -=
                    activity.getResources()
                            .getDimensionPixelSize(R.dimen.bookmark_popup_image_size);
        } else {
            mPropertyModel.set(BookmarkPopupProperties.IMAGE_VISIBLE, true);
        }

        ViewRectProvider rectProvider = new ViewRectProvider(anchor);
        rectProvider.setIncludePadding(true);

        mPopupWindow =
                new AnchoredPopupWindow.Builder(
                                activity,
                                anchor,
                                AppCompatResources.getDrawable(activity, R.drawable.menu_bg_tinted),
                                () -> mView,
                                rectProvider)
                        .setOutsideTouchable(true)
                        .setFocusable(true)
                        .setTouchModal(true)
                        .setMaxWidth(popupWidth)
                        .setDesiredContentWidth(popupWidth)
                        .setDismissOnScreenSizeChange(true)
                        .setHorizontalOverlapAnchor(true)
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

    /**
     * Shows the popup for the given bookmark.
     *
     * @param bookmarkId The ID of the bookmark to show.
     * @param launchType The user intent that opened the popup.
     * @param isNewBookmark Whether the bookmark was just created.
     */
    public void show(
            BookmarkId bookmarkId, @BookmarkPopupLaunchType int launchType, boolean isNewBookmark) {
        mMediator.show(bookmarkId, launchType, isNewBookmark);
        mPopupWindow.show();
        mView.focusTitleInput();
    }

    /** Destroys the coordinator, dismissing the popup. */
    public void destroy() {
        mPopupWindow.dismiss();
    }

    /** Returns the popup window for testing. */
    public AnchoredPopupWindow getPopupWindowForTesting() {
        return mPopupWindow;
    }

    /** Returns the property model for testing. */
    PropertyModel getPropertyModelForTesting() {
        return mPropertyModel;
    }

    /** Returns the view for testing. */
    BookmarkPopupView getViewForTesting() {
        return mView;
    }
}
