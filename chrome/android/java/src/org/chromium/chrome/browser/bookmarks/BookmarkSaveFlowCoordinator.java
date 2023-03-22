// Copyright 2021 The Chromium Authors
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

import org.chromium.base.lifetime.DestroyChecker;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.commerce.PriceTrackingUtils;
import org.chromium.chrome.browser.commerce.ShoppingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

/** Coordinates the bottom-sheet saveflow. */
public class BookmarkSaveFlowCoordinator {
    private static final int AUTODISMISS_TIME_MS = 6000;

    private final Context mContext;
    private final PropertyModel mPropertyModel =
            new PropertyModel(BookmarkSaveFlowProperties.ALL_PROPERTIES);
    private final PropertyModelChangeProcessor<PropertyModel, ViewLookupCachingFrameLayout,
            PropertyKey> mChangeProcessor;
    private final DestroyChecker mDestroyChecker;
    private final Profile mProfile;

    private BottomSheetController mBottomSheetController;
    private BookmarkSaveFlowBottomSheetContent mBottomSheetContent;
    private BookmarkSaveFlowMediator mMediator;
    private View mBookmarkSaveFlowView;
    private BookmarkModel mBookmarkModel;
    private UserEducationHelper mUserEducationHelper;
    private boolean mClosedViaRunnable;

    /**
     * @param context The {@link Context} associated with this cooridnator.
     * @param bottomSheetController Allows displaying content in the bottom sheet.
     * @param shoppingService Allows un/subscribing for product updates, used for
     *         price-tracking.
     * @param userEducationHelper A means of triggering IPH.
     */
    public BookmarkSaveFlowCoordinator(@NonNull Context context,
            @NonNull BottomSheetController bottomSheetController, ShoppingService shoppingService,
            @NonNull UserEducationHelper userEducationHelper, Profile profile) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mUserEducationHelper = userEducationHelper;
        mBookmarkModel = BookmarkModel.getForProfile(Profile.getLastUsedRegularProfile());
        mDestroyChecker = new DestroyChecker();
        mProfile = profile;

        mBookmarkSaveFlowView = LayoutInflater.from(mContext).inflate(
                org.chromium.chrome.R.layout.bookmark_save_flow, /*root=*/null);
        mMediator = new BookmarkSaveFlowMediator(
                mBookmarkModel, mPropertyModel, mContext, this::close, shoppingService);
        mChangeProcessor = PropertyModelChangeProcessor.create(mPropertyModel,
                (ViewLookupCachingFrameLayout) mBookmarkSaveFlowView,
                new BookmarkSaveFlowViewBinder());
    }

    /**
     * Shows the save flow for a normal bookmark.
     * @param bookmarkId The {@link BookmarkId} which was saved.
     */
    public void show(BookmarkId bookmarkId) {
        show(bookmarkId, /*fromExplicitTrackUi=*/false, /*wasBookmarkMoved=*/false,
                /*isNewBookmark=*/false);
    }

    /**
     * Shows the bookmark save flow sheet.
     * @param bookmarkId The {@link BookmarkId} which was saved.
     * @param fromExplicitTrackUi Whether the bookmark was added via a dedicated tracking entry
     *         point. This will change the UI of the bookmark save flow, either adding type-specific
     *         text (e.g. price tracking text) or adding UI bits to allow users to upgrade a regular
     *         bookmark. This will be false when adding a normal bookmark.
     * @param wasBookmarkMoved Whether the save flow is shown as a reslult of a moved bookmark.
     * @param isNewBookmark Whether the bookmark is newly created.
     */
    public void show(BookmarkId bookmarkId, boolean fromExplicitTrackUi, boolean wasBookmarkMoved,
            boolean isNewBookmark) {
        mBookmarkModel.finishLoadingBookmarkModel(() -> {
            show(bookmarkId, fromExplicitTrackUi, wasBookmarkMoved, isNewBookmark,
                    mBookmarkModel.getPowerBookmarkMeta(bookmarkId));
        });
    }

    void show(BookmarkId bookmarkId, boolean fromExplicitTrackUi, boolean wasBookmarkMoved,
            boolean isNewBookmark, @Nullable PowerBookmarkMeta meta) {
        mDestroyChecker.checkNotDestroyed();
        mBottomSheetContent = new BookmarkSaveFlowBottomSheetContent(mBookmarkSaveFlowView);
        // Order matters here: Calling show on the mediator first allows the height to be fully
        // determined before the sheet is shown.
        mMediator.show(bookmarkId, meta, fromExplicitTrackUi, wasBookmarkMoved, isNewBookmark);
        boolean shown =
                mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true);

        AccessibilityManager am =
                (AccessibilityManager) mContext.getSystemService(Context.ACCESSIBILITY_SERVICE);
        if (!am.isTouchExplorationEnabled()) {
            setupAutodismiss();
        }

        if (ShoppingFeatures.isShoppingListEligible()) {
            PriceTrackingUtils.isBookmarkPriceTracked(mProfile, bookmarkId.getId(), (isTracked) -> {
                if (isTracked) return;

                if (shown) {
                    showShoppingSaveFlowIPH();
                } else {
                    mBottomSheetController.addObserver(new EmptyBottomSheetObserver() {
                        @Override
                        public void onSheetContentChanged(BottomSheetContent newContent) {
                            if (newContent == mBottomSheetContent) {
                                showShoppingSaveFlowIPH();
                            }

                            mBottomSheetController.removeObserver(this);
                        }
                    });
                }
            });
        }
    }

    /**
     * Show the IPH for the save flow that tells a user that they can organize their products from
     * the bookmarks surface.
     */
    private void showShoppingSaveFlowIPH() {
        mUserEducationHelper.requestShowIPH(
                new IPHCommandBuilder(mBookmarkSaveFlowView.getResources(),
                        FeatureConstants.SHOPPING_LIST_SAVE_FLOW_FEATURE,
                        R.string.iph_shopping_list_save_flow, R.string.iph_shopping_list_save_flow)
                        .setAnchorView(
                                mBookmarkSaveFlowView.findViewById(R.id.bookmark_select_folder))
                        .build());
    }

    @VisibleForTesting
    void close() {
        mClosedViaRunnable = true;
        mBottomSheetController.hideContent(mBottomSheetContent, true);
    }

    private void setupAutodismiss() {
        PostTask.postDelayedTask(TaskTraits.UI_USER_VISIBLE, this::close, AUTODISMISS_TIME_MS);
    }

    private void destroy() {
        mDestroyChecker.checkNotDestroyed();
        mDestroyChecker.destroy();

        // The bottom sheet was closed by a means other than one of the edit actions.
        if (mClosedViaRunnable) {
            RecordUserAction.record("MobileBookmark.SaveFlow.ClosedWithoutEditAction");
        }

        mMediator.destroy();
        mMediator = null;

        mBookmarkSaveFlowView = null;

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

        @Override
        public boolean hasCustomScrimLifecycle() {
            return true;
        }
    }

    @VisibleForTesting
    View getViewForTesting() {
        return mBookmarkSaveFlowView;
    }

    boolean getIsDestroyedForTesting() {
        return mDestroyChecker.isDestroyed();
    }
}
