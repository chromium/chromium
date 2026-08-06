// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.widget.CompoundButton;

import org.chromium.base.CallbackController;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkMetrics.PriceTrackingState;
import org.chromium.chrome.browser.commerce.PriceTrackingUtils;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.SubscriptionsObserver;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the desktop android bookmark popup. */
@NullMarked
public class BookmarkPopupMediator implements SubscriptionsObserver {
    private final PropertyModel mPropertyModel;
    private final BookmarkModel mBookmarkModel;
    private final BookmarkManagerOpener mBookmarkManagerOpener;
    private final BookmarkImageFetcher mBookmarkImageFetcher;
    private final Context mContext;
    private final Profile mProfile;
    private final Runnable mDismissRunnable;
    private final CallbackController mCallbackController = new CallbackController();
    private final PriceDropNotificationManager mPriceDropNotificationManager;
    private final @Nullable ShoppingService mShoppingService;
    private @Nullable BookmarkId mBookmarkId;
    private @Nullable CommerceSubscription mSubscription;
    private String mCurrentTitle = "";

    /**
     * Constructs the BookmarkPopupMediator. This logic controller is responsible for binding
     * backend metadata (like price tracking attributes and folder hierarchies) to the popup
     * property model.
     *
     * @param propertyModel The {@link PropertyModel} to populate with bookmark details.
     * @param bookmarkModel The {@link BookmarkModel} to retrieve details and save edits.
     * @param bookmarkManagerOpener Interface to open the bookmark manager.
     * @param bookmarkImageFetcher Fetches icons/images for the bookmark. The mediator takes
     *     ownership of this object and is responsible for destroying it.
     * @param context The Android context.
     * @param profile The current Profile.
     * @param shoppingService Shopping service to fetch price tracking info if available.
     * @param priceDropNotificationManager Manager to handle price drop notifications.
     * @param dismissRunnable Runnable to execute when dismissing the popup.
     */
    public BookmarkPopupMediator(
            PropertyModel propertyModel,
            BookmarkModel bookmarkModel,
            BookmarkManagerOpener bookmarkManagerOpener,
            BookmarkImageFetcher bookmarkImageFetcher,
            Context context,
            Profile profile,
            @Nullable ShoppingService shoppingService,
            PriceDropNotificationManager priceDropNotificationManager,
            Runnable dismissRunnable) {
        mPropertyModel = propertyModel;
        mBookmarkModel = bookmarkModel;
        mBookmarkManagerOpener = bookmarkManagerOpener;
        mBookmarkImageFetcher = bookmarkImageFetcher;
        mContext = context;
        mProfile = profile;
        mDismissRunnable = dismissRunnable;

        mShoppingService = shoppingService;
        mPriceDropNotificationManager = priceDropNotificationManager;
        if (mShoppingService != null) {
            mShoppingService.addSubscriptionsObserver(this);
        }

        mPropertyModel.set(
                BookmarkPopupProperties.REMOVE_BUTTON_CLICK_LISTENER, this::onRemoveClicked);
        mPropertyModel.set(BookmarkPopupProperties.CLOSE_BUTTON_CLICK_LISTENER, mDismissRunnable);
        mPropertyModel.set(BookmarkPopupProperties.DONE_BUTTON_CLICK_LISTENER, this::onDoneClicked);
        mPropertyModel.set(BookmarkPopupProperties.TITLE_CHANGED_LISTENER, this::onTitleChanged);
        mPropertyModel.set(
                BookmarkPopupProperties.FOLDER_ROW_CLICK_LISTENER, this::onFolderRowClicked);
    }

    /** Destroys the mediator, cancelling any pending callbacks. */
    public void destroy() {
        mCallbackController.destroy();
        mBookmarkImageFetcher.destroy();

        if (mShoppingService != null) {
            mShoppingService.removeSubscriptionsObserver(this);
        }
    }

    /**
     * Shows the bookmark edit popup for the given bookmark.
     *
     * @param bookmarkId The ID of the bookmark to show.
     * @param isNewBookmark Whether this popup is for a newly added bookmark.
     */
    public void show(BookmarkId bookmarkId, boolean isNewBookmark) {
        mBookmarkId = bookmarkId;
        mPropertyModel.set(
                BookmarkPopupProperties.HEADER_TEXT,
                mContext.getString(
                        isNewBookmark ? R.string.bookmark_added : R.string.edit_bookmark));
        mBookmarkModel.finishLoadingBookmarkModel(
                mCallbackController.makeCancelable(
                        () -> {
                            BookmarkItem item = mBookmarkModel.getBookmarkById(bookmarkId);
                            if (item != null) {
                                mCurrentTitle = item.getTitle();
                                mPropertyModel.set(BookmarkPopupProperties.TITLE, mCurrentTitle);
                                BookmarkItem parent =
                                        mBookmarkModel.getBookmarkById(item.getParentId());
                                if (parent != null) {
                                    mPropertyModel.set(
                                            BookmarkPopupProperties.FOLDER_NAME, parent.getTitle());
                                }

                                PowerBookmarkMeta meta =
                                        mBookmarkModel.getPowerBookmarkMeta(bookmarkId);
                                bindPowerBookmarkProperties(meta);

                                // Pass 0 as the imageSize to fetch the original image/favicon size
                                // without any downscaling constraints, allowing the ImageView to
                                // scale it automatically using its layout bounds.
                                mBookmarkImageFetcher.fetchImageForBookmarkWithFaviconFallback(
                                        item,
                                        /* imageSize= */ 0,
                                        mCallbackController.makeCancelable(
                                                (Drawable drawable) ->
                                                        mPropertyModel.set(
                                                                BookmarkPopupProperties
                                                                        .IMAGE_DRAWABLE,
                                                                drawable)));
                            }
                        }));
    }

    private void bindPowerBookmarkProperties(@Nullable PowerBookmarkMeta meta) {
        if (meta == null
                || !meta.hasShoppingSpecifics()
                || mBookmarkId == null
                || mShoppingService == null) return;

        mSubscription = PowerBookmarkUtils.createCommerceSubscriptionForPowerBookmarkMeta(meta);

        mPropertyModel.set(BookmarkPopupProperties.PRICE_TRACKING_ENABLED, true);
        mPropertyModel.set(BookmarkPopupProperties.PRICE_TRACKING_SWITCH_CHECKED, false);
        mPropertyModel.set(BookmarkPopupProperties.PRICE_TRACKING_VISIBLE, true);
        mPropertyModel.set(
                BookmarkPopupProperties.PRICE_TRACKING_SWITCH_LISTENER,
                this::handlePriceTrackingSwitchToggle);

        PriceTrackingUtils.isBookmarkPriceTracked(
                mProfile,
                mBookmarkId.getId(),
                mCallbackController.makeCancelable(
                        (Boolean subscribed) -> {
                            setPriceTrackingToggleVisualsOnly(subscribed);
                            PowerBookmarkMetrics.reportBookmarkSaveFlowPriceTrackingState(
                                    PriceTrackingState.PRICE_TRACKING_SHOWN);
                        }));
    }

    private void handlePriceTrackingSwitchToggle(CompoundButton view, boolean toggled) {
        if (mBookmarkId == null) return;

        setPriceTrackingToggleVisualsOnly(toggled);
        PriceTrackingUtils.setPriceTrackingStateForBookmark(
                mProfile,
                mBookmarkId.getId(),
                toggled,
                mCallbackController.makeCancelable(
                        (Boolean success) -> {
                            if (!success) {
                                setPriceTrackingToggleVisualsOnly(!toggled);
                            }
                        }));

        PowerBookmarkMetrics.reportBookmarkSaveFlowPriceTrackingState(
                toggled
                        ? PriceTrackingState.PRICE_TRACKING_ENABLED
                        : PriceTrackingState.PRICE_TRACKING_DISABLED);
    }

    private void setPriceTrackingToggleVisualsOnly(boolean enabled) {
        mPropertyModel.set(BookmarkPopupProperties.PRICE_TRACKING_SWITCH_LISTENER, null);
        mPropertyModel.set(BookmarkPopupProperties.PRICE_TRACKING_SWITCH_CHECKED, enabled);
        mPropertyModel.set(
                BookmarkPopupProperties.PRICE_TRACKING_SWITCH_LISTENER,
                this::handlePriceTrackingSwitchToggle);
    }

    @Override
    public void onSubscribe(CommerceSubscription subscription, boolean succeeded) {
        if (!succeeded || !subscription.equals(mSubscription)) return;

        setPriceTrackingToggleVisualsOnly(true);
        mPriceDropNotificationManager.createNotificationChannel();
    }

    @Override
    public void onUnsubscribe(CommerceSubscription subscription, boolean succeeded) {
        if (!succeeded || !subscription.equals(mSubscription)) return;

        setPriceTrackingToggleVisualsOnly(false);
    }

    /** Returns the {@link BookmarkId} of the bookmark currently loaded, or null. */
    public @Nullable BookmarkId getBookmarkId() {
        return mBookmarkId;
    }

    private void onTitleChanged(String title) {
        mCurrentTitle = title;
    }

    private void onRemoveClicked() {
        if (mBookmarkId != null) {
            mBookmarkModel.deleteBookmark(mBookmarkId);
        }
        mDismissRunnable.run();
    }

    private void onFolderRowClicked() {
        if (mBookmarkId != null) {
            mBookmarkManagerOpener.startEditActivity(mContext, mProfile, mBookmarkId);
        }
        mDismissRunnable.run();
    }

    private void onDoneClicked() {
        if (mBookmarkId != null && mCurrentTitle != null) {
            mBookmarkModel.setBookmarkTitle(mBookmarkId, mCurrentTitle);
        }
        mDismissRunnable.run();
    }
}
