// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.widget.CompoundButton;

import androidx.annotation.Nullable;
import androidx.core.content.res.ResourcesCompat;

import com.google.common.primitives.UnsignedLongs;

import org.chromium.base.CallbackController;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkModelObserver;
import org.chromium.chrome.browser.power_bookmarks.PowerBookmarkMeta;
import org.chromium.chrome.browser.power_bookmarks.PowerBookmarkType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.CommerceSubscriptionType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.SubscriptionManagementType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.TrackingIdType;
import org.chromium.chrome.browser.subscriptions.SubscriptionsManager;
import org.chromium.components.bookmarks.BookmarkId;
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
    private boolean mFromExplicitTrackUi;
    private SubscriptionsManager mSubscriptionsManager;
    private CommerceSubscription mSubscription;

    /**
     * @param bookmarkModel The {@link BookmarkModel} which supplies the data.
     * @param propertyModel The {@link PropertyModel} which allows the mediator to push data to the
     *         model.
     * @param context The {@link Context} associated with this mediator.
     * @param closeRunnable A {@link Runnable} which closes the bookmark save flow.
     * @param subscriptionsManager Used to manage the price-tracking subscriptions.
     */
    public BookmarkSaveFlowMediator(BookmarkModel bookmarkModel, PropertyModel propertyModel,
            Context context, Runnable closeRunnable, SubscriptionsManager subscriptionsManager) {
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
     */
    public void show(
            BookmarkId bookmarkId, @Nullable PowerBookmarkMeta meta, boolean fromExplicitTrackUi) {
        RecordUserAction.record("MobileBookmark.SaveFlow.Show");

        mBookmarkId = bookmarkId;
        mPowerBookmarkMeta = meta;
        mFromExplicitTrackUi = fromExplicitTrackUi;

        mPropertyModel.set(BookmarkSaveFlowProperties.EDIT_ONCLICK_LISTENER, (v) -> {
            RecordUserAction.record("MobileBookmark.SaveFlow.EditBookmark");
            BookmarkUtils.startEditActivity(mContext, mBookmarkId);
            mCloseRunnable.run();
        });
        mPropertyModel.set(BookmarkSaveFlowProperties.FOLDER_SELECT_ONCLICK_LISTENER, (v) -> {
            RecordUserAction.record("MobileBookmark.SaveFlow.EditFolder");
            BookmarkUtils.startFolderSelectActivity(mContext, mBookmarkId);
            mCloseRunnable.run();
        });

        if (meta != null) {
            // Use UnsignedLongs to convert ProductClusterId to avoid overflow.
            mSubscription = new CommerceSubscription(CommerceSubscriptionType.PRICE_TRACK,
                    UnsignedLongs.toString(
                            mPowerBookmarkMeta.getShoppingSpecifics().getProductClusterId()),
                    SubscriptionManagementType.USER_MANAGED, TrackingIdType.PRODUCT_CLUSTER_ID);
        }
        bindBookmarkProperties(mBookmarkId, mPowerBookmarkMeta, mFromExplicitTrackUi);
        bindPowerBookmarkProperties(mBookmarkId, mPowerBookmarkMeta, mFromExplicitTrackUi);
    }

    private void bindBookmarkProperties(
            BookmarkId bookmarkId, PowerBookmarkMeta meta, boolean fromExplicitTrackUi) {
        BookmarkItem item = mBookmarkModel.getBookmarkById(bookmarkId);
        String folderName = mBookmarkModel.getBookmarkTitle(item.getParentId());
        mPropertyModel.set(BookmarkSaveFlowProperties.TITLE_TEXT,
                BookmarkUtils.getSaveFlowTitleForBookmark(mContext, bookmarkId, meta));
        mPropertyModel.set(BookmarkSaveFlowProperties.FOLDER_SELECT_ICON,
                BookmarkUtils.getFolderIcon(mContext, bookmarkId.getType()));
        mPropertyModel.set(BookmarkSaveFlowProperties.FOLDER_SELECT_ICON_ENABLED, item.isMovable());
        mPropertyModel.set(BookmarkSaveFlowProperties.SUBTITLE_TEXT,
                mContext.getResources().getString(
                        R.string.bookmark_page_saved_location, folderName));
    }

    private void bindPowerBookmarkProperties(
            BookmarkId bookmarkId, @Nullable PowerBookmarkMeta meta, boolean fromExplicitTrackUi) {
        if (meta == null) return;

        if (meta.getType() == PowerBookmarkType.SHOPPING) {
            if (fromExplicitTrackUi) {
                // TODO(crbug.com/1243383): Follow-up with UX about failing to subscribe.
                mSubscriptionsManager.subscribe(mSubscription, (status) -> {});
                return;
            }

            mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_VISIBLE, true);
            mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_START_ICON,
                    ResourcesCompat.getDrawable(mContext.getResources(),
                            R.drawable.price_tracking_enabled, /*theme=*/null));
            mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TITLE,
                    mContext.getResources().getString(
                            R.string.price_tracking_save_flow_notification_switch_title));
            mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_SUBTITLE,
                    mContext.getResources().getString(
                            R.string.price_tracking_save_flow_notification_switch_subtitle));
            mPropertyModel.set(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLE_LISTENER,
                    this::handleNotificationSwitchToggle);
        }
    }

    void handleNotificationSwitchToggle(CompoundButton view, boolean toggled) {
        if (toggled) {
            mSubscriptionsManager.subscribe(
                    mSubscription, mCallbackController.makeCancelable((status) -> {
                        // TODO(crbug.com/1243383): Follow-up with UX about failure.
                        if (status != SubscriptionsManager.StatusCode.OK) {
                            view.setChecked(false);
                        }
                    }));
        } else {
            mSubscriptionsManager.unsubscribe(
                    mSubscription, mCallbackController.makeCancelable((status) -> {
                        // TODO(crbug.com/1243383): Follow-up with UX about failure.
                        if (status != SubscriptionsManager.StatusCode.OK) {
                            view.setChecked(true);
                        }
                    }));
        }
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
        bindBookmarkProperties(mBookmarkId, mPowerBookmarkMeta, mFromExplicitTrackUi);
    }
}
