// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.CancelableRunnable;
import org.chromium.base.lifetime.DestroyChecker;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.commerce.PriceTrackingUtils;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinates the bottom-sheet saveflow. */
@NullMarked
public class BookmarkSaveFlowCoordinator implements ActivityStateListener {
    private static final int AUTO_DISMISS_TIME_MS = 10000;

    private final Activity mActivity;
    private final PropertyModel mPropertyModel;
    private final PropertyModelChangeProcessor<PropertyModel, ? extends View, PropertyKey>
            mChangeProcessor;
    private final DestroyChecker mDestroyChecker;
    private final Profile mProfile;

    private final BottomSheetController mBottomSheetController;
    private @Nullable BookmarkSaveFlowBottomSheetContent mBottomSheetContent;
    private BookmarkSaveFlowMediator mMediator;
    private View mBookmarkSaveFlowView;
    private final BookmarkModel mBookmarkModel;
    private final UserEducationHelper mUserEducationHelper;
    private boolean mClosedViaRunnable;
    private @Nullable CancelableRunnable mAutoDismissTask;

    /**
     * @param activity The hosting {@link Activity}. The coordinator listens for this activity's
     *     destruction to release its resources.
     * @param bottomSheetController Allows displaying content in the bottom sheet.
     * @param shoppingService Allows un/subscribing for product updates, used for price-tracking.
     * @param userEducationHelper A means of triggering IPH.
     * @param profile The current chrome profile.
     * @param identityManager The {@link IdentityManager} which supplies the account data.
     * @param bookmarkManagerOpener Manaages opening bookmarkms.
     * @param priceDropNotificationManager Manages price drop notifications.
     */
    public BookmarkSaveFlowCoordinator(
            Activity activity,
            BottomSheetController bottomSheetController,
            ShoppingService shoppingService,
            UserEducationHelper userEducationHelper,
            Profile profile,
            IdentityManager identityManager,
            BookmarkManagerOpener bookmarkManagerOpener,
            PriceDropNotificationManager priceDropNotificationManager) {
        mActivity = activity;
        mBottomSheetController = bottomSheetController;
        mUserEducationHelper = userEducationHelper;
        mBookmarkModel = BookmarkModel.getForProfile(profile);
        assert mBookmarkModel != null;
        mDestroyChecker = new DestroyChecker();
        mProfile = profile;

        mPropertyModel = new PropertyModel(ImprovedBookmarkSaveFlowProperties.ALL_KEYS);
        mBookmarkSaveFlowView =
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.improved_bookmark_save_flow, /* root= */ null);
        mChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mPropertyModel,
                        (ImprovedBookmarkSaveFlowView) mBookmarkSaveFlowView,
                        ImprovedBookmarkSaveFlowViewBinder::bind);

        BookmarkImageFetcher bookmarkImageFetcher =
                new BookmarkImageFetcher(
                        profile,
                        activity,
                        mBookmarkModel,
                        ImageFetcherFactory.createImageFetcher(
                                ImageFetcherConfig.DISK_CACHE_ONLY, mProfile.getProfileKey()),
                        BookmarkViewUtils.getRoundedIconGenerator(
                                mActivity, BookmarkRowDisplayPref.VISUAL));

        mMediator =
                new BookmarkSaveFlowMediator(
                        mBookmarkModel,
                        mPropertyModel,
                        mActivity,
                        this::close,
                        shoppingService,
                        bookmarkImageFetcher,
                        mProfile,
                        identityManager,
                        bookmarkManagerOpener,
                        priceDropNotificationManager);

        // Register for activity destruction so the coordinator (and its observer registration
        // on the long-lived BookmarkModel via the mediator) is released when the activity dies.
        ApplicationStatus.registerStateListenerForActivity(this, mActivity);
    }

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        // The bottom sheet's content destroy() is not called when the activity is torn down,
        // so do our own cleanup here.
        if (newState == ActivityState.DESTROYED) {
            destroy();
        }
    }

    /**
     * Shows the save flow for a normal bookmark.
     *
     * @param bookmarkId The {@link BookmarkId} which was saved.
     */
    public void show(BookmarkId bookmarkId) {
        show(
                bookmarkId,
                /* fromExplicitTrackUi= */ false,
                /* wasBookmarkMoved= */ false,
                /* isNewBookmark= */ false);
    }

    /**
     * Shows the bookmark save flow sheet.
     *
     * @param bookmarkId The {@link BookmarkId} which was saved.
     * @param fromExplicitTrackUi Whether the bookmark was added via a dedicated tracking entry
     *     point. This will change the UI of the bookmark save flow, either adding type-specific
     *     text (e.g. price tracking text) or adding UI bits to allow users to upgrade a regular
     *     bookmark. This will be false when adding a normal bookmark.
     * @param wasBookmarkMoved Whether the save flow is shown as a result of a moved bookmark.
     * @param isNewBookmark Whether the bookmark is newly created.
     */
    public void show(
            BookmarkId bookmarkId,
            boolean fromExplicitTrackUi,
            boolean wasBookmarkMoved,
            boolean isNewBookmark) {
        mBookmarkModel.finishLoadingBookmarkModel(
                () -> {
                    show(
                            bookmarkId,
                            fromExplicitTrackUi,
                            wasBookmarkMoved,
                            isNewBookmark,
                            mBookmarkModel.getPowerBookmarkMeta(bookmarkId));
                });
    }

    void show(
            BookmarkId bookmarkId,
            boolean fromExplicitTrackUi,
            boolean wasBookmarkMoved,
            boolean isNewBookmark,
            @Nullable PowerBookmarkMeta meta) {
        mDestroyChecker.checkNotDestroyed();
        mBottomSheetContent = new BookmarkSaveFlowBottomSheetContent(mBookmarkSaveFlowView);
        // Order matters here: Calling show on the mediator first allows the height to be fully
        // determined before the sheet is shown.
        mMediator.show(bookmarkId, meta, fromExplicitTrackUi, wasBookmarkMoved, isNewBookmark);
        boolean shown =
                mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true);

        if (!AccessibilityState.isTouchExplorationEnabled()) {
            setupAutodismiss();
        }

        if (CommerceFeatureUtils.isShoppingListEligible(
                ShoppingServiceFactory.getForProfile(mProfile))) {
            PriceTrackingUtils.isBookmarkPriceTracked(
                    mProfile,
                    bookmarkId.getId(),
                    (isTracked) -> {
                        if (isTracked) return;

                        if (shown) {
                            showShoppingSaveFlowIph();
                        } else {
                            mBottomSheetController.addObserver(
                                    new EmptyBottomSheetObserver() {
                                        @Override
                                        public void onSheetContentChanged(
                                                @Nullable BottomSheetContent newContent) {
                                            if (newContent == mBottomSheetContent) {
                                                showShoppingSaveFlowIph();
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
    private void showShoppingSaveFlowIph() {
        mUserEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                mBookmarkSaveFlowView.getResources(),
                                FeatureConstants.SHOPPING_LIST_SAVE_FLOW_FEATURE,
                                R.string.iph_shopping_list_save_flow,
                                R.string.iph_shopping_list_save_flow)
                        .setAnchorView(mBookmarkSaveFlowView.findViewById(R.id.edit_chev))
                        .build());
    }

    @VisibleForTesting
    void close() {
        mClosedViaRunnable = true;
        mBottomSheetController.hideContent(mBottomSheetContent, true);
    }

    private void setupAutodismiss() {
        mAutoDismissTask = new CancelableRunnable(this::close);
        PostTask.postDelayedTask(
                TaskTraits.UI_USER_VISIBLE, mAutoDismissTask, AUTO_DISMISS_TIME_MS);
    }

    @SuppressWarnings("NullAway")
    private void destroy() {
        mDestroyChecker.destroy();

        ApplicationStatus.unregisterActivityStateListener(this);

        // Cancel the auto-dismiss task so it stops retaining this coordinator (and the
        // activity it transitively holds) via the static TaskRunnerImpl task queue.
        if (mAutoDismissTask != null) {
            mAutoDismissTask.cancel();
            mAutoDismissTask = null;
        }

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

        @Override
        public @Nullable View getToolbarView() {
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
        public float getFullHeightRatio() {
            return BottomSheetContent.HeightMode.WRAP_CONTENT;
        }

        @Override
        public boolean swipeToDismissEnabled() {
            return true;
        }

        @Override
        public String getSheetContentDescription(Context context) {
            return context.getString(
                    R.string.bookmarks_save_flow_content_description, mMediator.getFolderName());
        }

        @Override
        public @StringRes int getSheetClosedAccessibilityStringId() {
            return R.string.bookmarks_save_flow_closed_description;
        }

        @Override
        public @StringRes int getSheetHalfHeightAccessibilityStringId() {
            return R.string.bookmark_save_flow_title;
        }

        @Override
        public @StringRes int getSheetFullHeightAccessibilityStringId() {
            return R.string.bookmark_save_flow_title;
        }

        @Override
        public boolean hasCustomScrimLifecycle() {
            return true;
        }
    }

    View getViewForTesting() {
        return mBookmarkSaveFlowView;
    }

    boolean getIsDestroyedForTesting() {
        return mDestroyChecker.isDestroyed();
    }
}
