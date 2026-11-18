// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.widget.CompoundButton;

import androidx.annotation.StringRes;

import org.chromium.base.CallbackController;
import org.chromium.base.TriState;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkDesktopPopupMetrics.BookmarkDesktopPopupOutcome;
import org.chromium.chrome.browser.bookmarks.BookmarkPopupCoordinator.BookmarkPopupLaunchType;
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
    private boolean mIsNewBookmark;
    private boolean mOutcomeRecorded;
    // The switch state from the latest user toggle (reverted on request failure), or NOT_SET if the
    // user has not toggled the switch since the popup was shown.
    private @TriState int mUserToggledPriceTrackingState = TriState.NOT_SET;
    private int mToggleRequestId;
    private @BookmarkPopupLaunchType int mLaunchType;

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
        recordOutcome(BookmarkDesktopPopupOutcome.DISMISSED);

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
     * @param launchType The user intent that opened the popup.
     * @param isNewBookmark Whether this popup is for a newly added bookmark.
     */
    public void show(
            BookmarkId bookmarkId, @BookmarkPopupLaunchType int launchType, boolean isNewBookmark) {
        mIsNewBookmark = isNewBookmark;
        mOutcomeRecorded = false;
        mUserToggledPriceTrackingState = TriState.NOT_SET;
        mToggleRequestId = 0;
        // Price tracking needs the shopping service; without it this is a plain bookmark popup.
        mLaunchType = mShoppingService == null ? BookmarkPopupLaunchType.BOOKMARK : launchType;
        mBookmarkId = bookmarkId;
        // Price tracking launches optimistically show the switch and header in the tracking state.
        setPriceTrackingSwitchAndHeader(isPriceTrackingLaunch());
        // Request the subscribe before finishLoadingBookmarkModel so dismissing the popup before
        // the model loads cannot skip it.
        if (isPriceTrackingEnableLaunch()) {
            enablePriceTracking(bookmarkId);
        }
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

    private boolean isPriceTrackingEnableLaunch() {
        return mLaunchType == BookmarkPopupLaunchType.PRICE_TRACKING_ENABLE;
    }

    private boolean isPriceTrackingLaunch() {
        return mLaunchType == BookmarkPopupLaunchType.PRICE_TRACKING_ENABLE
                || mLaunchType == BookmarkPopupLaunchType.PRICE_TRACKING_MANAGE;
    }

    private void bindPowerBookmarkProperties(@Nullable PowerBookmarkMeta meta) {
        if (mBookmarkId == null || mShoppingService == null) return;

        if (meta != null && meta.hasShoppingSpecifics()) {
            mSubscription = PowerBookmarkUtils.createCommerceSubscriptionForPowerBookmarkMeta(meta);
        } else if (!isPriceTrackingLaunch()) {
            return;
        }
        // Price tracking launch types may lack shopping meta (e.g. a just-created bookmark); the
        // switch is shown optimistically. Without meta, mSubscription stays null for the popup's
        // lifetime, so subscription observer callbacks are ignored.

        mPropertyModel.set(BookmarkPopupProperties.PRICE_TRACKING_ENABLED, true);
        mPropertyModel.set(BookmarkPopupProperties.PRICE_TRACKING_VISIBLE, true);
        mPropertyModel.set(
                BookmarkPopupProperties.PRICE_TRACKING_SWITCH_LISTENER,
                this::handlePriceTrackingSwitchToggle);

        // ENABLE launches already requested the subscribe in show(); keep the optimistic switch.
        if (isPriceTrackingEnableLaunch()) return;

        PriceTrackingUtils.isBookmarkPriceTracked(
                mProfile,
                mBookmarkId.getId(),
                mCallbackController.makeCancelable(
                        (Boolean subscribed) -> {
                            if (mUserToggledPriceTrackingState != TriState.NOT_SET) return;

                            setPriceTrackingSwitchAndHeader(Boolean.TRUE.equals(subscribed));
                        }));
    }

    /** Subscribes the bookmark to price tracking, reverting the optimistic switch on failure. */
    private void enablePriceTracking(BookmarkId bookmarkId) {
        mPriceDropNotificationManager.createNotificationChannel();
        PriceTrackingUtils.setPriceTrackingStateForBookmark(
                mProfile,
                bookmarkId.getId(),
                /* enabled= */ true,
                mCallbackController.makeCancelable(
                        (Boolean success) -> {
                            if (Boolean.TRUE.equals(success)
                                    || mUserToggledPriceTrackingState != TriState.NOT_SET) {
                                return;
                            }

                            setPriceTrackingSwitchAndHeader(/* enabled= */ false);
                        }));
    }

    private void handlePriceTrackingSwitchToggle(CompoundButton view, boolean toggled) {
        if (mBookmarkId == null) return;

        // Stamp each toggle so a late failure from an older request cannot revert a newer one. It
        // would also overwrite mUserToggledPriceTrackingState, so the newer toggle's observer echo
        // would be dropped as stale.
        int requestId = ++mToggleRequestId;
        mUserToggledPriceTrackingState = toggled ? TriState.TRUE : TriState.FALSE;
        setPriceTrackingSwitchAndHeader(toggled);
        PriceTrackingUtils.setPriceTrackingStateForBookmark(
                mProfile,
                mBookmarkId.getId(),
                toggled,
                mCallbackController.makeCancelable(
                        (Boolean success) -> {
                            if (!Boolean.TRUE.equals(success) && requestId == mToggleRequestId) {
                                // Keep the stored state in sync with the reverted switch so
                                // observer echoes of the real state are not dropped as stale.
                                mUserToggledPriceTrackingState =
                                        toggled ? TriState.FALSE : TriState.TRUE;
                                setPriceTrackingSwitchAndHeader(!toggled);
                            }
                        }));
    }

    /** Sets the switch without notifying its listener, and the header that follows it. */
    private void setPriceTrackingSwitchAndHeader(boolean enabled) {
        mPropertyModel.set(BookmarkPopupProperties.PRICE_TRACKING_SWITCH_LISTENER, null);
        mPropertyModel.set(BookmarkPopupProperties.PRICE_TRACKING_SWITCH_CHECKED, enabled);
        mPropertyModel.set(
                BookmarkPopupProperties.PRICE_TRACKING_SWITCH_LISTENER,
                this::handlePriceTrackingSwitchToggle);
        updateHeader(enabled);
    }

    /** Updates the header title based on the launch type and price tracking state. */
    private void updateHeader(boolean trackingOn) {
        @StringRes
        int headerResId =
                isPriceTrackingLaunch() && trackingOn
                        ? R.string.price_tracking_title
                        : (mIsNewBookmark ? R.string.bookmark_added : R.string.edit_bookmark);
        mPropertyModel.set(BookmarkPopupProperties.HEADER_TEXT, mContext.getString(headerResId));
    }

    @Override
    public void onSubscribe(CommerceSubscription subscription, boolean succeeded) {
        if (!succeeded
                || !subscription.equals(mSubscription)
                || isReplacedByUserToggle(/* subscribed= */ true)) {
            return;
        }

        setPriceTrackingSwitchAndHeader(/* enabled= */ true);
        mPriceDropNotificationManager.createNotificationChannel();
    }

    @Override
    public void onUnsubscribe(CommerceSubscription subscription, boolean succeeded) {
        if (!succeeded
                || !subscription.equals(mSubscription)
                || isReplacedByUserToggle(/* subscribed= */ false)) {
            return;
        }

        setPriceTrackingSwitchAndHeader(/* enabled= */ false);
    }

    /**
     * Returns whether an observer echo of {@code subscribed} is stale because a later user toggle
     * requested the opposite state, e.g. an auto-subscribe success landing after the user turned
     * the switch off.
     */
    private boolean isReplacedByUserToggle(boolean subscribed) {
        return mUserToggledPriceTrackingState == (subscribed ? TriState.FALSE : TriState.TRUE);
    }

    /** Returns the {@link BookmarkId} of the bookmark currently loaded, or null. */
    public @Nullable BookmarkId getBookmarkId() {
        return mBookmarkId;
    }

    private void onTitleChanged(String title) {
        mCurrentTitle = title;
    }

    private void onRemoveClicked() {
        recordOutcome(BookmarkDesktopPopupOutcome.REMOVED);
        if (mBookmarkId != null) {
            mBookmarkModel.deleteBookmark(mBookmarkId);
        }
        mDismissRunnable.run();
    }

    private void onFolderRowClicked() {
        recordOutcome(BookmarkDesktopPopupOutcome.EDIT_DIALOG_OPENED);
        if (mBookmarkId != null) {
            mBookmarkManagerOpener.startEditActivity(mContext, mProfile, mBookmarkId);
        }
        mDismissRunnable.run();
    }

    private void onDoneClicked() {
        recordOutcome(BookmarkDesktopPopupOutcome.SAVED);
        if (mBookmarkId != null && mCurrentTitle != null) {
            mBookmarkModel.setBookmarkTitle(mBookmarkId, mCurrentTitle);
        }
        mDismissRunnable.run();
    }

    private void recordOutcome(@BookmarkDesktopPopupOutcome int outcome) {
        if (mOutcomeRecorded) return;
        mOutcomeRecorded = true;
        BookmarkDesktopPopupMetrics.recordOutcome(outcome, mIsNewBookmark);
    }
}
