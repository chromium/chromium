// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the tab bottom sheet. */
@NullMarked
public class TabBottomSheetCoordinator {
    private final ComponentCallbacks mComponentsCallbacks =
            new ComponentCallbacks() {
                @Override
                public void onConfigurationChanged(Configuration configuration) {
                    if (mIsSheetCurrentlyManagedByController) {
                        mExpectingLayoutChange = true;
                    }
                }

                @Override
                public void onLowMemory() {}
            };

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final PropertyModel mModel;
    private final CoBrowseViews mCoBrowseViews;
    private final TabBottomSheetMediator mMediator;
    private final WindowAndroid mWindowAndroid;

    private @Nullable Runnable mOnClose;
    private @Nullable TabBottomSheetContent mSheetContent;
    private @Nullable BottomSheetObserver mSheetObserver;
    private @Nullable PropertyModelChangeProcessor mViewBinder;
    private @Nullable View mContentView;

    private boolean mIsSheetCurrentlyManagedByController;
    private boolean mExpectingLayoutChange;

    /**
     * @param context The context to use for creating views.
     * @param bottomSheetController The {@link BottomSheetController} used to show the bottom sheet.
     * @param coBrowseViews The views to be displayed within the bottom sheet. These should be
     *     obtained via {@link CoBrowseViewFactory}. Note that these views have a single-use
     *     lifecycle; they are destroyed when the bottom sheet is closed and cannot be reused for
     *     subsequent showings.
     * @param onClose The callback to be invoked when the bottom sheet is closed.
     */
    TabBottomSheetCoordinator(
            Context context,
            WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController,
            CoBrowseViews coBrowseViews,
            @Nullable Runnable onClose) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mBottomSheetController = bottomSheetController;
        mCoBrowseViews = coBrowseViews;
        mOnClose = onClose;

        mModel = TabBottomSheetProperties.createDefaultModel(coBrowseViews);

        mMediator = new TabBottomSheetMediator(mContext, mModel, coBrowseViews);

        coBrowseViews.setWebUiTouchHandler(mMediator.getWebUiTouchHandler());
    }

    /** Tries to show the bottom sheet. */
    boolean tryToShowBottomSheet(boolean animate, boolean startsExpanded) {
        if (mIsSheetCurrentlyManagedByController) {
            return false;
        }
        mContentView = mCoBrowseViews.getView();
        mSheetContent = new TabBottomSheetContent(mContentView);
        mViewBinder =
                PropertyModelChangeProcessor.create(
                        mModel, mContentView, TabBottomSheetViewBinder::bind);

        if (mBottomSheetController.requestShowContent(mSheetContent, animate)) {
            // Set peek height for touch arbitration.
            mMediator.setPeekHeight(mSheetContent.getPeekHeight());

            // If bottom sheet has never been initialized, its max height return 0.
            // We set it here, and if it changes later, we will update it in the observer.
            mContentView.post(
                    () -> {
                        boolean isKeyboard = isKeyboardShowing();
                        mMediator.setMaxSheetHeight(
                                mBottomSheetController.getContainerHeight(), isKeyboard);
                        if (startsExpanded
                                && mSheetContent != null
                                && mMediator.isSheetHeightSufficient()) {
                            mBottomSheetController.expandSheet();
                        }
                    });

            mSheetObserver = buildBottomSheetObserver();
            mBottomSheetController.addObserver(mSheetObserver);

            mContext.registerComponentCallbacks(mComponentsCallbacks);

            mIsSheetCurrentlyManagedByController = true;
            return true;
        } else {
            // This happens when either.
            // 1) If the sheet content is null.
            // 2) The bottom sheet is null.
            // 3) If its being shown, or is in queue but not currently shown.
            // 4) If a sheet of higher priority came up.
            cleanupSheetResources();
            return false;
        }
    }

    /**
     * Attaches the peek view to the bottom sheet.
     *
     * @param peekView The peek view to attach.
     */
    void attachPeekView(View peekView) {
        mCoBrowseViews.attachPeekView(peekView);
    }

    /**
     * Shows the peek view and hides the expanded content.
     *
     * @return Whether the peek view was successfully shown.
     */
    boolean showPeekViewAndHideExpandedContent() {
        if (!mCoBrowseViews.hasPeekView()) {
            return false;
        }
        mMediator.onSheetStateChanged(SheetState.PEEK, mCoBrowseViews.hasPeekView());
        return true;
    }

    /**
     * Hides the peek view and shows the expanded content.
     *
     * @return Whether the peek view was successfully hidden.
     */
    boolean hidePeekViewAndShowExpandedContent() {
        if (!mCoBrowseViews.hasPeekView()) {
            return false;
        }
        mMediator.onSheetStateChanged(SheetState.FULL, mCoBrowseViews.hasPeekView());
        return true;
    }

    void closeBottomSheet() {
        if (!mIsSheetCurrentlyManagedByController) {
            return;
        }
        mBottomSheetController.hideContent(mSheetContent, false, StateChangeReason.NONE);
    }

    boolean isSheetShowing() {
        return mIsSheetCurrentlyManagedByController;
    }

    // Cleanup methods.
    void destroy() {
        if (mIsSheetCurrentlyManagedByController && mSheetContent != null) {
            mBottomSheetController.hideContent(mSheetContent, false, StateChangeReason.NONE);
        } else {
            // Inside else block since this will be called when the bottom sheet is hidden.
            cleanupSheetResources();
        }
        if (mOnClose != null) {
            mOnClose = null;
        }
    }

    private void cleanupSheetResources() {
        if (mSheetObserver != null && mBottomSheetController != null) {
            mBottomSheetController.removeObserver(mSheetObserver);
            mSheetObserver = null;
        }

        mContext.unregisterComponentCallbacks(mComponentsCallbacks);

        if (mSheetContent != null) {
            mSheetContent.destroy();
            mSheetContent = null;
        }
        if (mViewBinder != null) {
            mViewBinder.destroy();
            mViewBinder = null;
        }
        mIsSheetCurrentlyManagedByController = false;
    }

    // Observer methods.
    private BottomSheetObserver buildBottomSheetObserver() {
        return new EmptyBottomSheetObserver() {
            @Override
            public void onSheetStateChanged(@SheetState int state, @StateChangeReason int reason) {
                if (mSheetContent == null) return;
                mMediator.onSheetStateChanged(state, mCoBrowseViews.hasPeekView());
                if (state == SheetState.HIDDEN) {
                    cleanupSheetResources();
                    if (mOnClose != null) {
                        mOnClose.run();
                    }
                }
            }

            @Override
            public void onContainerSizeChanged(int containerWidth, int containerHeight) {
                if (mExpectingLayoutChange) {
                    mBottomSheetController.collapseSheet(/* animate= */ true);
                    mExpectingLayoutChange = false;
                }
                mMediator.setMaxSheetHeight(containerHeight, isKeyboardShowing());
            }
        };
    }

    private boolean isKeyboardShowing() {
        if (mWindowAndroid.getKeyboardDelegate() == null) return false;
        return mWindowAndroid.getKeyboardDelegate().isKeyboardShowing(mCoBrowseViews.getView());
    }

    // Testing methods.
    PropertyModel getModelForTesting() {
        return mModel;
    }

    boolean isSheetCurrentlyManagedForTesting() {
        return mIsSheetCurrentlyManagedByController;
    }

    boolean isExpectingLayoutChangeForTesting() {
        return mExpectingLayoutChange;
    }
}
