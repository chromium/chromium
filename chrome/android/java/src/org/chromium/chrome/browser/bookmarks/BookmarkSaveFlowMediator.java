// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.os.Build;
import android.widget.CompoundButton;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
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

import java.util.List;

/** Controls the bookmarks save-flow. */
public class BookmarkSaveFlowMediator
        extends BookmarkModelObserver implements SubscriptionsObserver {
    private final Context mContext;
    private final Runnable mCloseRunnable;

    private CallbackController mCallbackController = new CallbackController();
    private PropertyModel mPropertyModel;
    private BookmarkModel mBookmarkModel;
    private BookmarkId mBookmarkId;
    private PowerBookmarkMeta mPowerBookmarkMeta;
    private boolean mWasBookmarkMoved;
    private boolean mIsNewBookmark;
    private ShoppingService mShoppingService;
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
     */
    public BookmarkSaveFlowMediator(BookmarkModel bookmarkModel, PropertyModel propertyModel,
            Context context, Runnable closeRunnable, ShoppingService shoppingService) {
        mBookmarkModel = bookmarkModel;
        mBookmarkModel.addObserver(this);

        mPropertyModel = propertyModel;
        mContext = context;
        mCloseRunnable = closeRunnable;

        mShoppingService = shoppingService;
        if (mShoppingService != null) {
            mShoppingService.addSubscriptionsObserver(this);
        }
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

        mPropertyModel.set(BookmarkSaveFlowProperties.EDIT_ONCLICK_LISTENER, (v) -> {
            RecordUserAction.record("MobileBookmark.SaveFlow.EditBookmark");
            BookmarkUtils.startEditActivity(mContext, mBookmarkId);
            mCloseRunnable.run();
        });
        mPropertyModel.set(BookmarkSaveFlowProperties.FOLDER_SELECT_ONCLICK_LISTENER, (v) -> {
            RecordUserAction.record("MobileBookmark.SaveFlow.EditFolder");
            BookmarkUtils.startFolderSelectActivity(mContext, mBookmarkId);
            TrackerFactory.getTrackerForProfile(Profile.getLastUsedRegularProfile())
                    .notifyEvent(EventConstants.SHOPPING_LIST_SAVE_FLOW_FOLDER_TAP);
            mCloseRunnable.run();
        });

        if (meta != null) {
            mSubscription = PowerBookmarkUtils.createCommerceSubscriptionForPowerBookmarkMeta(meta);
        }
        bindBookmarkProperties(mBookmarkId, mPowerBookmarkMeta, mWasBookmarkMoved);
        bindPowerBookmarkProperties(mBookmarkId, mPowerBookmarkMeta, fromExplicitTrackUi);
    }

    private void bindBookmarkProperties(
            BookmarkId bookmarkId, PowerBookmarkMeta meta, boolean wasBookmarkMoved) {
        BookmarkItem item = mBookmarkModel.getBookmarkById(bookmarkId);
        mFolderName = mBookmarkModel.getBookmarkTitle(item.getParentId());
        mPropertyModel.set(BookmarkSaveFlowProperties.TITLE_TEXT,
                mContext.getResources().getString(wasBookmarkMoved
                                ? R.string.bookmark_save_flow_title_move
                                : R.string.bookmark_save_flow_title));
        mPropertyModel.set(BookmarkSaveFlowProperties.FOLDER_SELECT_ICON,
                BookmarkUtils.getFolderIcon(mContext, bookmarkId.getType()));
        mPropertyModel.set(BookmarkSaveFlowProperties.FOLDER_SELECT_ICON_ENABLED,
                BookmarkUtils.isMovable(item));
        mPropertyModel.set(BookmarkSaveFlowProperties.SUBTITLE_TEXT,
                mContext.getResources().getString(wasBookmarkMoved
                                ? R.string.bookmark_page_moved_location
                                : R.string.bookmark_page_saved_location,
                        mFolderName));
    }

    private void bindPowerBookmarkProperties(
            BookmarkId bookmarkId, @Nullable PowerBookmarkMeta meta, boolean fromExplicitTrackUi) {
        if (meta == null) return;

        if (meta.hasShoppingSpecifics()) {
            setPriceTrackingNotificationUiEnabled(true);
            setPriceTrackingIconForEnabledState(false);
            mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_VISIBLE, true);
            mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TITLE,
                    mContext.getResources().getString(R.string.enable_price_tracking_menu_item));
            mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLE_LISTENER,
                    this::handleNotificationSwitchToggle);

            if (fromExplicitTrackUi) {
                mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLED, true);
            }
            PowerBookmarkMetrics.reportBookmarkSaveFlowPriceTrackingState(
                    PriceTrackingState.PRICE_TRACKING_SHOWN);
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
        PriceTrackingUtils.setPriceTrackingStateForBookmark(Profile.getLastUsedRegularProfile(),
                mBookmarkId.getId(), toggled, mSubscriptionsManagerCallback, mIsNewBookmark);
        PowerBookmarkMetrics.reportBookmarkSaveFlowPriceTrackingState(toggled
                        ? PriceTrackingState.PRICE_TRACKING_ENABLED
                        : PriceTrackingState.PRICE_TRACKING_DISABLED);
    }

    void setPriceTrackingNotificationUiEnabled(boolean enabled) {
        mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_UI_ENABLED, enabled);
        mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_SUBTITLE,
                mContext.getResources().getString(enabled
                                ? R.string.price_tracking_save_flow_notification_switch_subtitle
                                : R.string.price_tracking_save_flow_notification_switch_subtitle_error));
    }

    void setPriceTrackingIconForEnabledState(boolean enabled) {
        mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_START_ICON_RES,
                enabled ? R.drawable.price_tracking_enabled_filled
                        : R.drawable.price_tracking_disabled);
    }

    void destroy() {
        mBookmarkModel.removeObserver(this);
        if (mShoppingService != null) {
            mShoppingService.removeSubscriptionsObserver(this);
        }

        mBookmarkModel = null;
        mPropertyModel = null;
        mBookmarkId = null;

        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
    }

    @VisibleForTesting
    void setPriceTrackingToggleVisualsOnly(boolean enabled) {
        mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLE_LISTENER, null);
        mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLED, enabled);
        setPriceTrackingIconForEnabledState(enabled);
        mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLE_LISTENER,
                this::handleNotificationSwitchToggle);
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
        bindBookmarkProperties(mBookmarkId, mPowerBookmarkMeta, mWasBookmarkMoved);
    }

    // SubscriptionsObserver implementation
    @Override
    public void onSubscribe(List<CommerceSubscription> subscriptions, boolean succeeded) {
        if (!succeeded) return;
        setPriceTrackingToggleVisualsOnly(subscriptions.contains(mSubscription));
    }

    @Override
    public void onUnsubscribe(List<CommerceSubscription> subscriptions, boolean succeeded) {
        if (!succeeded) return;
        setPriceTrackingToggleVisualsOnly(!subscriptions.contains(mSubscription));
    }
}
