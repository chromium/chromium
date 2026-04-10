// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.components.browser_ui.widget.TouchEventProvider;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the tab bottom sheet. */
@NullMarked
public class TabBottomSheetCoordinator {
    // Values are not final and may need tuning.
    private static final float FLING_VELOCITY_THRESHOLD_DP = 50f;
    private static final float SCROLL_DISTANCE_THRESHOLD_DP = 100f;

    // Can be modified later to be set dynamically based on device
    private static final float FULL_HEIGHT_RATIO = 0.7f;
    private static final float KEYBOARD_SHOWING_HEIGHT_RATIO = 0.9f;

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

    private final GestureDetector mGestureDetector;
    private final GestureDetector.SimpleOnGestureListener mGestureListener =
            new GestureDetector.SimpleOnGestureListener() {
                @Override
                public boolean onDown(MotionEvent e) {
                    return true;
                }

                @Override
                public boolean onDoubleTap(MotionEvent e) {
                    collapseSheet();
                    return true;
                }

                @Override
                public void onLongPress(MotionEvent e) {
                    collapseSheet();
                }

                @Override
                public boolean onScroll(
                        @Nullable MotionEvent e1,
                        MotionEvent e2,
                        float distanceX,
                        float distanceY) {
                    if (e1 == null || e2 == null) return false;
                    float totalDistanceY = e2.getRawY() - e1.getRawY();
                    if (Math.abs(totalDistanceY)
                            > ViewUtils.dpToPx(mContext, SCROLL_DISTANCE_THRESHOLD_DP)) {
                        collapseSheet();
                    }
                    return true;
                }

                @Override
                public boolean onFling(
                        @Nullable MotionEvent e1,
                        MotionEvent e2,
                        float velocityX,
                        float velocityY) {
                    if (Math.abs(velocityY)
                            > ViewUtils.dpToPx(mContext, FLING_VELOCITY_THRESHOLD_DP)) {
                        collapseSheet();
                    }
                    return true;
                }
            };

    private final TouchEventObserver mTouchEventObserver =
            new TouchEventObserver() {
                @Override
                public boolean mayInterceptTouchSequenceInWebContents() {
                    // Given that the bottom sheet only intercepts for very brief period of time,
                    // this is safe to return true. The alternatives suggested by the warning on
                    // this method are infeasible for this use case.
                    return true;
                }

                @Override
                public boolean onInterceptTouchEvent(MotionEvent e) {
                    // Intercept the touch stream if it's the start of a gesture, or process
                    // it normally to detect scrolls.
                    mGestureDetector.onTouchEvent(e);
                    // Do not claim the sequence by returning true here, just silently observe it.
                    return false;
                }
            };

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final TouchEventProvider mTouchEventProvider;
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
     * @param touchEventProvider The {@link TouchEventProvider} used to observe touch events on the
     *     tab behind the bottom sheet.
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
            TouchEventProvider touchEventProvider,
            CoBrowseViews coBrowseViews,
            @Nullable Runnable onClose) {
        mContext = context;
        mGestureDetector = new GestureDetector(mContext, mGestureListener);
        mWindowAndroid = windowAndroid;
        mBottomSheetController = bottomSheetController;
        mTouchEventProvider = touchEventProvider;
        mCoBrowseViews = coBrowseViews;
        mOnClose = onClose;

        mModel = TabBottomSheetProperties.createDefaultModel(coBrowseViews);

        mMediator =
                new TabBottomSheetMediator(
                        mContext,
                        mModel,
                        coBrowseViews,
                        FULL_HEIGHT_RATIO,
                        KEYBOARD_SHOWING_HEIGHT_RATIO);

        coBrowseViews.setWebUiTouchHandler(mMediator.getWebUiTouchHandler());
    }

    /** Tries to show the bottom sheet. */
    boolean tryToShowBottomSheet(boolean animate, boolean startsExpanded) {
        if (mIsSheetCurrentlyManagedByController) {
            return false;
        }
        mContentView = mCoBrowseViews.getView();
        mSheetContent = new TabBottomSheetContent(mContentView, getDefaultHeightRatio());
        mViewBinder =
                PropertyModelChangeProcessor.create(
                        mModel, mContentView, TabBottomSheetViewBinder::bind);

        if (mBottomSheetController.requestShowContent(mSheetContent, animate)) {
            // Set peek height for touch arbitration.
            mMediator.setPeekHeight(mSheetContent.getPeekHeight());

            // If bottom sheet has never been initialized, the max bottom offset may be 0.
            // We set it here, and if it changes later, we will update it in the observer.
            mContentView.post(
                    () -> {
                        updateResizingStateWithFixedHeight();

                        boolean isSheetHeightSufficient =
                                mMediator.isSheetHeightSufficient(
                                        mBottomSheetController.getMaxOffset());
                        if (startsExpanded && mSheetContent != null && isSheetHeightSufficient) {
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
        stopObservingCompositorViewInteractions();

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

                if (state == SheetState.HALF || state == SheetState.FULL) {
                    observeCompositorViewInteractions();
                } else {
                    stopObservingCompositorViewInteractions();
                }
            }

            @Override
            public void onContainerSizeChanged(int containerWidth, int containerHeight) {
                if (mExpectingLayoutChange) {
                    mBottomSheetController.collapseSheet(/* animate= */ true);
                    mExpectingLayoutChange = false;
                }
                updateResizingStateWithFixedHeight();
            }
        };
    }

    private void observeCompositorViewInteractions() {
        mTouchEventProvider.addTouchEventObserver(mTouchEventObserver);
    }

    private void stopObservingCompositorViewInteractions() {
        mTouchEventProvider.removeTouchEventObserver(mTouchEventObserver);
    }

    private void collapseSheet() {
        if (mBottomSheetController.getCurrentSheetContent() == mSheetContent) {
            mBottomSheetController.collapseSheet(/* animate= */ true);
        }
    }

    private boolean isKeyboardShowing() {
        KeyboardVisibilityDelegate keyboardDelegate = mWindowAndroid.getKeyboardDelegate();
        if (keyboardDelegate == null) return false;
        return keyboardDelegate.isKeyboardShowing(mCoBrowseViews.getView());
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

    GestureDetector.SimpleOnGestureListener getGestureListenerForTesting() {
        return mGestureListener;
    }

    private void updateResizingStateWithFixedHeight() {
        float defaultHeightRatio = getDefaultHeightRatio();
        mMediator.updateResizingState(
                defaultHeightRatio,
                defaultHeightRatio,
                mBottomSheetController.getCurrentOffset(),
                mBottomSheetController.getMaxOffset());
    }

    private float getDefaultHeightRatio() {
        return isKeyboardShowing() ? KEYBOARD_SHOWING_HEIGHT_RATIO : FULL_HEIGHT_RATIO;
    }
}
