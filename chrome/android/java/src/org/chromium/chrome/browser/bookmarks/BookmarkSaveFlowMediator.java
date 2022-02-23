// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.widget.CompoundButton;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkMetrics.PriceTrackingState;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.power_bookmarks.PowerBookmarkMeta;
import org.chromium.chrome.browser.power_bookmarks.PowerBookmarkType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription;
import org.chromium.chrome.browser.subscriptions.SubscriptionsManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.ui.modelutil.PropertyModel;

/** Controls the bookmarks save-flow. */
public class BookmarkSaveFlowMediator extends BookmarkModelObserver {
    private final Context mContext;
    private final Runnable mCloseRunnable;

    private CallbackController mCallbackController = new CallbackController();
    private PropertyModel mPropertyModel;
    private BookmarkModel mBookmarkModel;
    private BookmarkId mBookmarkId;
    private PowerBookmarkMeta mPowerBookmarkMeta;
    private boolean mWasBookmarkMoved;
    private SubscriptionsManager mSubscriptionsManager;
    private CommerceSubscription mSubscription;
    private Callback<Integer> mSubscriptionsManagerCallback;
    private String mFolderName;

    /**
     * @param bookmarkModel The {@link BookmarkModel} which supplies the data.
     * @param propertyModel The {@link PropertyModel} which allows the mediator to push data to the
     *         model.
     * @param context The {@link Context} associated with this mediator.
     * @param closeRunnable A {@link Runnable} which closes the bookmark save flow.
     * @param subscriptionsManager Used to manage the price-tracking subscriptions.
     */
    public BookmarkSaveFlowMediator(BookmarkModel bookmarkModel, PropertyModel propertyModel,
            Context context, Runnable closeRunnable,
            @Nullable SubscriptionsManager subscriptionsManager) {
        mBookmarkModel = bookmarkModel;
        mBookmarkModel.addObserver(this);

        mPropertyModel = propertyModel;
        mContext = context;
        mCloseRunnable = closeRunnable;

        mSubscriptionsManager = subscriptionsManager;
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
     * @param wasBookmarkMoved Whether the save flow is shown as a reslult of a moved bookmark.
     */
    public void show(BookmarkId bookmarkId, @Nullable PowerBookmarkMeta meta,
            boolean fromExplicitTrackUi, boolean wasBookmarkMoved) {
        RecordUserAction.record("MobileBookmark.SaveFlow.Show");

        mBookmarkId = bookmarkId;
        mPowerBookmarkMeta = meta;
        mWasBookmarkMoved = wasBookmarkMoved;

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
        mPropertyModel.set(BookmarkSaveFlowProperties.FOLDER_SELECT_ICON_ENABLED, item.isMovable());
        mPropertyModel.set(BookmarkSaveFlowProperties.SUBTITLE_TEXT,
                mContext.getResources().getString(wasBookmarkMoved
                                ? R.string.bookmark_page_moved_location
                                : R.string.bookmark_page_saved_location,
                        mFolderName));
    }

    private void bindPowerBookmarkProperties(
            BookmarkId bookmarkId, @Nullable PowerBookmarkMeta meta, boolean fromExplicitTrackUi) {
        if (meta == null) return;

        if (meta.getType() == PowerBookmarkType.SHOPPING) {
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
            mSubscriptionsManagerCallback = mCallbackController.makeCancelable((Integer status) -> {
                boolean statusOk = (status == SubscriptionsManager.StatusCode.OK);
                if (!statusOk) {
                    // Set it back to the previous state if the request.
                    mPropertyModel.set(
                            BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLE_LISTENER, null);
                    view.setChecked(!toggled);
                    mPropertyModel.set(
                            BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLE_LISTENER,
                            this::handleNotificationSwitchToggle);
                    setPriceTrackingIconForEnabledState(!toggled);
                }
                setPriceTrackingNotificationUiEnabled(statusOk);
            });
        }

        setPriceTrackingIconForEnabledState(toggled);
        PowerBookmarkUtils.setPriceTrackingEnabled(mSubscriptionsManager, mBookmarkModel,
                mBookmarkId, toggled, mSubscriptionsManagerCallback);
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
        mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_START_ICON,
                AppCompatResources.getDrawable(mContext,
                        enabled ? R.drawable.price_tracking_enabled_filled
                                : R.drawable.price_tracking_disabled));
    }

    void destroy() {
        mBookmarkModel.removeObserver(this);
        mBookmarkModel = null;
        mPropertyModel = null;
        mBookmarkId = null;

        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
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
}
