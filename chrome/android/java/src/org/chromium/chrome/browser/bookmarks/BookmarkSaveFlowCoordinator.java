// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.accessibility.AccessibilityManager;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackController;
import org.chromium.base.lifetime.DestroyChecker;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.power_bookmarks.PowerBookmarkMeta;
import org.chromium.chrome.browser.subscriptions.SubscriptionsManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

/** Coordinates the bottom-sheet saveflow. */
public class BookmarkSaveFlowCoordinator {
    private final Context mContext;
    private final PropertyModel mPropertyModel =
            new PropertyModel(BookmarkSaveFlowProperties.ALL_PROPERTIES);
    private final PropertyModelChangeProcessor<PropertyModel, ViewLookupCachingFrameLayout,
            PropertyKey> mChangeProcessor;
    private final DestroyChecker mDestroyChecker;

    private CallbackController mCallbackController = new CallbackController();
    private BottomSheetController mBottomSheetController;
    private BookmarkSaveFlowBottomSheetContent mBottomSheetContent;
    private BookmarkSaveFlowMediator mMediator;
    private View mBookmarkSaveFlowView;

    private BookmarkModel mBookmarkModel;

    private boolean mClosedViaRunnable;

    /**
     * @param context The {@link Context} associated with this cooridnator.
     * @param bottomSheetController Allows displaying content in the bottom sheet.
     * @param subscriptionsManager Allows un/subscribing for product updates, used for
     *         price-tracking.
     */
    public BookmarkSaveFlowCoordinator(@NonNull Context context,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull SubscriptionsManager subscriptionsManager) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mBookmarkModel = new BookmarkModel();
        mDestroyChecker = new DestroyChecker();

        mBookmarkSaveFlowView = LayoutInflater.from(mContext).inflate(
                org.chromium.chrome.R.layout.bookmark_save_flow, /*root=*/null);
        mMediator = new BookmarkSaveFlowMediator(
                mBookmarkModel, mPropertyModel, mContext, this::close, subscriptionsManager);
        mChangeProcessor = PropertyModelChangeProcessor.create(mPropertyModel,
                (ViewLookupCachingFrameLayout) mBookmarkSaveFlowView,
                new BookmarkSaveFlowViewBinder());
    }

    /**
     * Shows the save flow for a normal bookmark.
     * @param bookmarkId The {@link BookmarkId} which was saved.
     */
    public void show(BookmarkId bookmarkId) {
        show(bookmarkId, /*fromExplicitTrackUi=*/false);
    }

    /**
     * Shows the bookmark save flow sheet.
     * @param bookmarkId The {@link BookmarkId} which was saved.
     * @param fromExplicitTrackUi Whether the bookmark was added via a dedicated tracking entry
     *         point. This will change the UI of the bookmark save flow, either adding type-specific
     *         text (e.g. price tracking text) or adding UI bits to allow users to upgrade a regular
     *         bookmark. This will be false when adding a normal bookmark.
     */
    public void show(BookmarkId bookmarkId, boolean fromExplicitTrackUi) {
        assert mBookmarkModel.isBookmarkModelLoaded();
        show(bookmarkId, fromExplicitTrackUi, mBookmarkModel.getPowerBookmarkMeta(bookmarkId));
    }

    void show(
            BookmarkId bookmarkId, boolean fromExplicitTrackUi, @Nullable PowerBookmarkMeta meta) {
        mDestroyChecker.checkNotDestroyed();
        mBottomSheetContent = new BookmarkSaveFlowBottomSheetContent(mBookmarkSaveFlowView);
        mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true);
        mMediator.show(bookmarkId, meta, fromExplicitTrackUi);

        AccessibilityManager am =
                (AccessibilityManager) mContext.getSystemService(Context.ACCESSIBILITY_SERVICE);
        if (!am.isTouchExplorationEnabled()) {
            setupAutodismiss();
        }
    }

    private void close() {
        mDestroyChecker.checkNotDestroyed();

        mClosedViaRunnable = true;
        mBottomSheetController.hideContent(mBottomSheetContent, true);
    }

    private void setupAutodismiss() {
        if (!BookmarkFeatures.isImprovedSaveFlowAutodismissEnabled()) return;

        PostTask.postDelayedTask(UiThreadTaskTraits.USER_VISIBLE,
                mCallbackController.makeCancelable(this::close),
                BookmarkFeatures.getImprovedSaveFlowAutodismissTimeMs());
    }

    private void destroy() {
        mDestroyChecker.checkNotDestroyed();
        mDestroyChecker.destroy();

        // The bottom sheet was closed by a means other than one of the edit actions.
        if (mClosedViaRunnable) {
            RecordUserAction.record("MobileBookmark.SaveFlow.ClosedWithoutEditAction");
        }
        mCallbackController.destroy();

        mMediator.destroy();
        mMediator = null;

        mBookmarkSaveFlowView = null;

        mBookmarkModel.destroy();
        mBookmarkModel = null;

        mChangeProcessor.destroy();
    }

    private class BookmarkSaveFlowBottomSheetContent implements BottomSheetContent {
        private final View mContentView;

        BookmarkSaveFlowBottomSheetContent(View contentView) {
            mContentView = contentView;
        }

        @Override
        public View getContentView() {
            return mContentView;
        }

        @Nullable
        @Override
        public View getToolbarView() {
            return null;
        }

        @Override
        public int getVerticalScrollOffset() {
            return 0;
        }

        @Override
        public void destroy() {
            BookmarkSaveFlowCoordinator.this.destroy();
        }

        @Override
        public int getPriority() {
            return BottomSheetContent.ContentPriority.HIGH;
        }

        @Override
        public int getPeekHeight() {
            return BottomSheetContent.HeightMode.DISABLED;
        }

        @Override
        public float getFullHeightRatio() {
            return BottomSheetContent.HeightMode.WRAP_CONTENT;
        }

        @Override
        public boolean swipeToDismissEnabled() {
            return true;
        }

        @Override
        public int getSheetContentDescriptionStringId() {
            return R.string.bookmarks_save_flow_content_description;
        }

        @Override
        public int getSheetClosedAccessibilityStringId() {
            return R.string.bookmarks_save_flow_closed_description;
        }

        @Override
        public int getSheetHalfHeightAccessibilityStringId() {
            return R.string.bookmarks_save_flow_opened_half;
        }

        @Override
        public int getSheetFullHeightAccessibilityStringId() {
            return R.string.bookmarks_save_flow_opened_full;
        }
    }

    @VisibleForTesting
    View getViewForTesting() {
        return mBookmarkSaveFlowView;
    }
}
