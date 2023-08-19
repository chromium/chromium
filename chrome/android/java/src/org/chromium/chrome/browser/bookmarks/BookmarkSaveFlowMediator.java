// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.view.View;
import android.widget.CompoundButton;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkSaveFlowProperties.FolderText;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkMetrics.PriceTrackingState;
import org.chromium.chrome.browser.commerce.PriceTrackingUtils;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.SubscriptionsObserver;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * Controls the bookmarks save-flow, which has 2 variants: standard, improved.
 * The two variants have different properties, so each of the methods is branched to reflect that.
 * BookmarkSaveFlowProperties shouldn't be used for the improved variant (it'll crash), and the
 * same is true for ImprovedBookmarkSaveFlow properties with the standard variant.
 * standard: The default save experience prior to android-improved-bookmarks.
 * improved: The new experience for saving when android-improved-bookmarks is enabled.
 */
public class BookmarkSaveFlowMediator
        extends BookmarkModelObserver implements SubscriptionsObserver {
    private static final String FOLDER_TEXT_TOKEN = "%1$s";
    private final Context mContext;
    private final Runnable mCloseRunnable;
    private final BookmarkImageFetcher mBookmarkImageFetcher;
    private final CallbackController mCallbackController = new CallbackController();
    private final PropertyModel mPropertyModel;
    private final BookmarkModel mBookmarkModel;
    private final ShoppingService mShoppingService;
    private final Profile mProfile;

    private BookmarkId mBookmarkId;
    private PowerBookmarkMeta mPowerBookmarkMeta;
    private boolean mWasBookmarkMoved;
    private boolean mIsNewBookmark;
    private CommerceSubscription mSubscription;
    private Callback<Boolean> mSubscriptionsManagerCallback;
    private String mFolderName;

    /**
     * @param bookmarkModel The {@link BookmarkModel} which supplies the data.
     * @param propertyModel The {@link PropertyModel} which allows the mediator to push data to the
     *         model.
     * @param context The {@link Context} associated with this mediator.
     * @param closeRunnable A {@link Runnable} which closes the bookmark save flow.
     * @param shoppingService Used to manage the price-tracking subscriptions.
     * @param bookmarkImageFetcher Used to fetch images/favicons for bookmarks.
     * @param profile The current chrome profile.
     */
    public BookmarkSaveFlowMediator(BookmarkModel bookmarkModel, PropertyModel propertyModel,
            Context context, Runnable closeRunnable, ShoppingService shoppingService,
            BookmarkImageFetcher bookmarkImageFetcher, Profile profile) {
        mBookmarkModel = bookmarkModel;
        mBookmarkModel.addObserver(this);

        mPropertyModel = propertyModel;
        mContext = context;
        mCloseRunnable = closeRunnable;

        mShoppingService = shoppingService;
        if (mShoppingService != null) {
            mShoppingService.addSubscriptionsObserver(this);
        }

        mBookmarkImageFetcher = bookmarkImageFetcher;
        mProfile = profile;
    }

    /**
     * Shows bottom sheet save-flow for the given {@link BookmarkId}.
     *
     * @param bookmarkId The {@link BookmarkId} to show.
     * @param meta The power bookmark metadata for the given BookmarkId.
     * @param fromExplicitTrackUi Whether the bookmark was added via a dedicated tracking entry
     *         point. This will change the UI of the bookmark save flow, either adding type-specific
     *         text (e.g. price tracking text) or adding UI bits to allow users to upgrade a regular
     *         bookmark.
     * @param wasBookmarkMoved Whether the save flow is shown as a result of a moved bookmark.
     * @param isNewBookmark Whether the bookmark is newly created.
     */
    public void show(BookmarkId bookmarkId, @Nullable PowerBookmarkMeta meta,
            boolean fromExplicitTrackUi, boolean wasBookmarkMoved, boolean isNewBookmark) {
        RecordUserAction.record("MobileBookmark.SaveFlow.Show");

        mBookmarkId = bookmarkId;
        mPowerBookmarkMeta = meta;
        mWasBookmarkMoved = wasBookmarkMoved;
        mIsNewBookmark = isNewBookmark;

        if (BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
            mPropertyModel.set(ImprovedBookmarkSaveFlowProperties.BOOKMARK_ROW_CLICK_LISTENER,
                    this::onEditClicked);
        } else {
            mPropertyModel.set(
                    BookmarkSaveFlowProperties.EDIT_ONCLICK_LISTENER, this::onEditClicked);
            mPropertyModel.set(BookmarkSaveFlowProperties.FOLDER_SELECT_ONCLICK_LISTENER,
                    this::onFolderSelectClicked);
        }

        if (meta != null) {
            mSubscription = PowerBookmarkUtils.createCommerceSubscriptionForPowerBookmarkMeta(meta);
        }

        BookmarkItem item = mBookmarkModel.getBookmarkById(bookmarkId);
        bindBookmarkProperties(item, mPowerBookmarkMeta, mWasBookmarkMoved);
        bindPowerBookmarkProperties(mPowerBookmarkMeta, fromExplicitTrackUi);
        if (BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
            bindImage(item, meta);
        }
    }

    private void bindBookmarkProperties(
            BookmarkItem item, PowerBookmarkMeta meta, boolean wasBookmarkMoved) {
        mFolderName = mBookmarkModel.getBookmarkTitle(item.getParentId());

        if (BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
            String folderDisplayTextRaw = getFolderDisplayTextRaw(wasBookmarkMoved);
            String folderDisplayText = getFolderDisplayText(wasBookmarkMoved);
            mPropertyModel.set(ImprovedBookmarkSaveFlowProperties.FOLDER_TEXT,
                    new FolderText(folderDisplayText,
                            folderDisplayTextRaw.indexOf(FOLDER_TEXT_TOKEN), mFolderName.length()));
        } else {
            mPropertyModel.set(BookmarkSaveFlowProperties.TITLE_TEXT,
                    mContext.getResources().getString(wasBookmarkMoved
                                    ? R.string.bookmark_save_flow_title_move
                                    : R.string.bookmark_save_flow_title));
            mPropertyModel.set(BookmarkSaveFlowProperties.FOLDER_SELECT_ICON,
                    BookmarkUtils.getFolderIcon(
                            mContext, item.getId().getType(), BookmarkRowDisplayPref.COMPACT));
            mPropertyModel.set(BookmarkSaveFlowProperties.FOLDER_SELECT_ICON_ENABLED,
                    BookmarkUtils.isMovable(mBookmarkModel, item));
            mPropertyModel.set(BookmarkSaveFlowProperties.SUBTITLE_TEXT,
                    getFolderDisplayText(wasBookmarkMoved));
        }
    }

    private void bindPowerBookmarkProperties(
            @Nullable PowerBookmarkMeta meta, boolean fromExplicitTrackUi) {
        if (meta == null) return;

        if (meta.hasShoppingSpecifics()) {
            setPriceTrackingNotificationUiEnabled(true);
            setPriceTrackingIconForEnabledState(false);
            if (BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
                mPropertyModel.set(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_VISIBLE, true);

                mPropertyModel.set(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_CHECKED,
                        fromExplicitTrackUi);
                mPropertyModel.set(
                        ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_LISTENER,
                        this::handleNotificationSwitchToggle);
                PowerBookmarkMetrics.reportBookmarkSaveFlowPriceTrackingState(
                        PriceTrackingState.PRICE_TRACKING_SHOWN);
            } else {
                mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_VISIBLE, true);
                mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TITLE,
                        mContext.getResources().getString(
                                R.string.enable_price_tracking_menu_item));
                mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLE_LISTENER,
                        this::handleNotificationSwitchToggle);

                if (fromExplicitTrackUi) {
                    mPropertyModel.set(
                            BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLED, true);
                }
                PowerBookmarkMetrics.reportBookmarkSaveFlowPriceTrackingState(
                        PriceTrackingState.PRICE_TRACKING_SHOWN);
            }
        }
    }

    void bindImage(BookmarkItem item, @Nullable PowerBookmarkMeta meta) {
        Callback<Drawable> callback = drawable -> {
            mPropertyModel.set(ImprovedBookmarkSaveFlowProperties.BOOKMARK_ROW_ICON, drawable);
        };

        if (meta != null && meta.hasShoppingSpecifics()) {
            mBookmarkImageFetcher.fetchImageUrlWithFallbacks(
                    new GURL(meta.getLeadImage().getUrl()), item, callback);
        } else {
            mBookmarkImageFetcher.fetchImageForBookmarkWithFaviconFallback(item, callback);
        }
    }

    void handleNotificationSwitchToggle(CompoundButton view, boolean toggled) {
        if (mSubscriptionsManagerCallback == null) {
            mSubscriptionsManagerCallback =
                    mCallbackController.makeCancelable((Boolean success) -> {
                        setPriceTrackingToggleVisualsOnly(success && view.isChecked());
                        setPriceTrackingNotificationUiEnabled(success);
                    });
        }

        // Make sure the notification channel is initialized when the user tracks a product.
        // TODO(crbug.com/1382191): Add a SubscriptionsObserver in the PriceDropNotificationManager
        // and initialize the channel there.
        if (toggled && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            PriceDropNotificationManagerFactory.create().createNotificationChannel();
        }
        setPriceTrackingIconForEnabledState(toggled);
        PriceTrackingUtils.setPriceTrackingStateForBookmark(mProfile, mBookmarkId.getId(), toggled,
                mSubscriptionsManagerCallback, mIsNewBookmark);
        PowerBookmarkMetrics.reportBookmarkSaveFlowPriceTrackingState(toggled
                        ? PriceTrackingState.PRICE_TRACKING_ENABLED
                        : PriceTrackingState.PRICE_TRACKING_DISABLED);
    }

    void setPriceTrackingNotificationUiEnabled(boolean enabled) {
        if (BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
            mPropertyModel.set(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_ENABLED, enabled);
        } else {
            mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_UI_ENABLED, enabled);
            mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_SUBTITLE,
                    mContext.getResources().getString(enabled
                                    ? R.string.price_tracking_save_flow_notification_switch_subtitle
                                    : R.string.price_tracking_save_flow_notification_switch_subtitle_error));
        }
    }

    void setPriceTrackingIconForEnabledState(boolean enabled) {
        if (!BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
            mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_START_ICON_RES,
                    enabled ? R.drawable.price_tracking_enabled_filled
                            : R.drawable.price_tracking_disabled);
        }
    }

    void destroy() {
        mBookmarkModel.removeObserver(this);
        if (mShoppingService != null) {
            mShoppingService.removeSubscriptionsObserver(this);
        }

        mBookmarkId = null;

        if (mCallbackController != null) {
            mCallbackController.destroy();
        }
    }

    @VisibleForTesting
    void setPriceTrackingToggleVisualsOnly(boolean enabled) {
        if (BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
            mPropertyModel.set(
                    ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_LISTENER, null);
            mPropertyModel.set(
                    ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_CHECKED, enabled);
            mPropertyModel.set(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_LISTENER,
                    this::handleNotificationSwitchToggle);
        } else {
            mPropertyModel.set(
                    BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLE_LISTENER, null);
            mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLED, enabled);
            setPriceTrackingIconForEnabledState(enabled);
            mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLE_LISTENER,
                    this::handleNotificationSwitchToggle);
        }
    }

    void setSubscriptionForTesting(CommerceSubscription subscription) {
        mSubscription = subscription;
    }

    // BookmarkModelObserver implementation

    @Override
    public void bookmarkModelChanged() {
        // Possibility that the bookmark is deleted while in accessibility mode.
        if (mBookmarkId == null || mBookmarkModel.getBookmarkById(mBookmarkId) == null) {
            mCloseRunnable.run();
            return;
        }

        BookmarkItem item = mBookmarkModel.getBookmarkById(mBookmarkId);
        bindBookmarkProperties(item, mPowerBookmarkMeta, mWasBookmarkMoved);
    }

    // SubscriptionsObserver implementation

    @Override
    public void onSubscribe(CommerceSubscription subscription, boolean succeeded) {
        if (!succeeded || !subscription.equals(mSubscription)) return;
        setPriceTrackingToggleVisualsOnly(true);
    }

    @Override
    public void onUnsubscribe(CommerceSubscription subscription, boolean succeeded) {
        if (!succeeded || !subscription.equals(mSubscription)) return;
        setPriceTrackingToggleVisualsOnly(false);
    }

    // Private functions

    private String getFolderDisplayTextRaw(boolean wasBookmarkMoved) {
        @StringRes
        int stringRes;
        if (wasBookmarkMoved) {
            stringRes = R.string.bookmark_page_moved_location;
        } else {
            stringRes = R.string.bookmark_page_saved_location;
        }

        return mContext.getString(stringRes);
    }

    private String getFolderDisplayText(boolean wasBookmarkMoved) {
        @StringRes
        int stringRes;
        if (wasBookmarkMoved) {
            stringRes = R.string.bookmark_page_moved_location;
        } else {
            stringRes = R.string.bookmark_page_saved_location;
        }

        return mContext.getString(stringRes, mFolderName);
    }

    private void onEditClicked(View v) {
        RecordUserAction.record("MobileBookmark.SaveFlow.EditBookmark");
        BookmarkUtils.startEditActivity(mContext, mBookmarkId);
        mCloseRunnable.run();
    }

    private void onFolderSelectClicked(View v) {
        RecordUserAction.record("MobileBookmark.SaveFlow.EditFolder");
        BookmarkUtils.startFolderSelectActivity(mContext, mBookmarkId);
        TrackerFactory.getTrackerForProfile(mProfile).notifyEvent(
                EventConstants.SHOPPING_LIST_SAVE_FLOW_FOLDER_TAP);
        mCloseRunnable.run();
    }
}
