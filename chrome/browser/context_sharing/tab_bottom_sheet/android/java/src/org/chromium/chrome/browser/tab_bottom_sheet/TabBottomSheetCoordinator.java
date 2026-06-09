// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetUtils.isActivityInactive;

import static java.lang.Math.max;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.view.accessibility.AccessibilityEvent;

import androidx.annotation.Px;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicMetrics;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.components.browser_ui.widget.TouchEventProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
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
    private static final float SMALL_SCREEN_HEIGHT_RATIO = 0.9f;
    private static final String TAG = "TabBottomSheet";

    // Interface used by the manager to monitor events related to the state of the
    // bottom sheet.
    interface SheetEventsCallback {
        /** Called when the bottom sheet is closed or suppressed. */
        void onBottomSheetClosed();

        /** Called when the bottom sheet is opened or when the bottom sheet state changes. */
        void onBottomSheetOpened(boolean isExpanded);
    }

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
                    if (e1 == null || e2 == null) {
                        return false;
                    }
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

    private final Callback<@Nullable WebContents> mWebContentsObserver =
            ignored -> updateInactivePlaceholder();

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final TouchEventProvider mTouchEventProvider;
    private final PropertyModel mModel;
    private final CoBrowseViews mCoBrowseViews;
    private final TabBottomSheetMediator mMediator;
    private final WindowAndroid mWindowAndroid;
    private final RoundedCornerOutlineProvider mOutlineProvider;
    private final Runnable mOnBackPressed;

    private @Nullable SheetEventsCallback mSheetEventsCallback;
    private @Nullable TabBottomSheetContent mSheetContent;
    private @Nullable BottomSheetObserver mSheetObserver;
    private @Nullable ComponentCallbacks mComponentsCallbacks;
    private @Nullable PropertyModelChangeProcessor mViewBinder;
    private @Nullable View mContentView;

    private boolean mIsShowingTabBottomSheet;
    private boolean mExpectingLayoutChange;
    private boolean mInitialContainerSizeChanged;

    private @Nullable KeyboardVisibilityListener mKeyboardVisibilityListener;
    private @Nullable ModalDialogManager mObservedModalDialogManager;
    private @Nullable ModalDialogManagerObserver mModalDialogManagerObserver;

    /**
     * @param context The context to use for creating views.
     * @param windowAndroid The {@link WindowAndroid} associated with the window.
     * @param bottomSheetController The {@link BottomSheetController} used to show the bottom sheet.
     * @param touchEventProvider The {@link TouchEventProvider} used to observe touch events on the
     *     tab behind the bottom sheet.
     * @param coBrowseViews The views to be displayed within the bottom sheet. These should be
     *     obtained via {@link CoBrowseViewFactory}. Note that these views have a single-use
     *     lifecycle; they are destroyed when the bottom sheet is closed and cannot be reused for
     *     subsequent showings.
     * @param sheetEventsCallback Interface used by the manager to monitor events related to the
     *     state of the bottom sheet.
     * @param onBackPressed Callback run when the back button/swipe is triggered.
     */
    TabBottomSheetCoordinator(
            Context context,
            WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController,
            TouchEventProvider touchEventProvider,
            CoBrowseViews coBrowseViews,
            SheetEventsCallback sheetEventsCallback,
            Runnable onBackPressed) {
        mContext = context;
        mGestureDetector = new GestureDetector(mContext, mGestureListener);
        mWindowAndroid = windowAndroid;
        mBottomSheetController = bottomSheetController;
        mTouchEventProvider = touchEventProvider;
        mCoBrowseViews = coBrowseViews;
        mSheetEventsCallback = sheetEventsCallback;
        mOnBackPressed = onBackPressed;

        mModel = TabBottomSheetProperties.createDefaultModel(coBrowseViews);

        mMediator =
                new TabBottomSheetMediator(
                        mContext,
                        mModel,
                        coBrowseViews,
                        FULL_HEIGHT_RATIO,
                        SMALL_SCREEN_HEIGHT_RATIO);

        coBrowseViews.setWebUiTouchHandler(mMediator.getWebUiTouchHandler());
        int radius =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.tab_bottom_sheet_peek_corner_radius);
        mOutlineProvider = new RoundedCornerOutlineProvider(radius);
        mCoBrowseViews.getWebContentsSupplier().addSyncObserver(mWebContentsObserver);
    }

    /** Tries to show the bottom sheet. */
    boolean tryToShowBottomSheet(boolean animate, boolean startsExpanded) {
        if (mIsShowingTabBottomSheet || mSheetEventsCallback == null) {
            return false;
        }
        if (mCoBrowseViews.hasPeekView()) {
            mMediator.onSheetStateChanged(startsExpanded ? SheetState.FULL : SheetState.PEEK);
        }
        mContentView = mCoBrowseViews.getView();
        mContentView.setOutlineProvider(mOutlineProvider);
        mContentView.setClipToOutline(true);
        TabBottomSheetContentProvider provider = mCoBrowseViews.getContentProvider();
        assert provider != null : "TabBottomSheetContentProvider must not be null";
        mSheetContent =
                provider.create(
                        mContentView,
                        FULL_HEIGHT_RATIO,
                        mCoBrowseViews.getBackgroundColor(),
                        mContentView
                                .getResources()
                                .getDimensionPixelSize(R.dimen.tab_bottom_sheet_peek_height_total),
                        R.id.peek_view_container,
                        R.id.empty_placeholder_container,
                        mOnBackPressed);
        mViewBinder =
                PropertyModelChangeProcessor.create(
                        mModel, mContentView, TabBottomSheetViewBinder::bind);

        if (mBottomSheetController.requestShowContent(mSheetContent, animate)) {
            // Set peek height for touch arbitration.
            if (mSheetContent != null) {
                mMediator.setPeekHeight(mSheetContent.getPeekHeight());
            }
            // Notify that the sheet is opened synchronously. The precise expansion state will be
            // refined once the posted task completes and layout is available.
            if (mSheetEventsCallback != null) {
                mSheetEventsCallback.onBottomSheetOpened(startsExpanded);
            }

            // If bottom sheet has never been initialized, the max bottom offset may be 0.
            // We set it here, and if it changes later, we will update it in the observer.
            mContentView.post(
                    () -> {
                        if (mSheetEventsCallback == null) {
                            return;
                        }
                        updateRoundingEdges();
                        setToFixedHeightOrFallback();

                        boolean isSheetHeightSufficient =
                                mMediator.isSheetHeightSufficient(getDesiredFixedHeight());
                        if (startsExpanded) {
                            if (mSheetContent != null && isSheetHeightSufficient) {
                                mBottomSheetController.expandSheet(animate);
                            } else {
                                mSheetEventsCallback.onBottomSheetOpened(/* isExpanded= */ false);
                            }
                        }
                    });

            if (mSheetObserver == null) {
                mSheetObserver = buildBottomSheetObserver();
                mBottomSheetController.addObserver(mSheetObserver);
            }
            if (mComponentsCallbacks == null) {
                mComponentsCallbacks = buildComponentsCallback();
                mContext.registerComponentCallbacks(mComponentsCallbacks);
            }

            if (mKeyboardVisibilityListener == null) {
                mKeyboardVisibilityListener = buildKeyboardVisibilityListener();
                mWindowAndroid
                        .getKeyboardDelegate()
                        .addKeyboardVisibilityListener(mKeyboardVisibilityListener);
            }

            if (mModalDialogManagerObserver == null) {
                ModalDialogManager modalDialogManager = mWindowAndroid.getModalDialogManager();
                if (modalDialogManager != null) {
                    mModalDialogManagerObserver = buildModalDialogManagerObserver();
                    modalDialogManager.addObserver(mModalDialogManagerObserver);
                    mObservedModalDialogManager = modalDialogManager;
                }
            }

            mIsShowingTabBottomSheet = true;
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
     * Removes the peek view from the bottom sheet.
     *
     * @param peekView The peek view to remove.
     */
    void removePeekView(View peekView) {
        mCoBrowseViews.removePeekView(peekView);
    }

    /**
     * Sets whether the bottom sheet is expanded.
     *
     * @param expanded Whether the bottom sheet should be expanded.
     */
    void setSheetExpanded(boolean expanded) {
        if (expanded) {
            mBottomSheetController.expandSheet();
        } else {
            mBottomSheetController.collapseSheet(/* animate= */ true);
        }
    }

    void closeBottomSheet(boolean animate) {
        mBottomSheetController.hideContent(mSheetContent, animate, StateChangeReason.NONE);
    }

    boolean isSheetShowing() {
        return mIsShowingTabBottomSheet;
    }

    boolean isInPeekMode() {
        return mBottomSheetController.getSheetState() == BottomSheetController.SheetState.PEEK;
    }

    // Cleanup methods.
    void destroy() {
        mCoBrowseViews.getWebContentsSupplier().removeObserver(mWebContentsObserver);
        if (mIsShowingTabBottomSheet && mSheetContent != null) {
            mBottomSheetController.hideContent(mSheetContent, false, StateChangeReason.NONE);
        }
        // Inside else block since this will be called when the bottom sheet is hidden.
        cleanupSheetResources();
    }

    private void cleanupSheetResources() {
        if (mSheetObserver != null && mBottomSheetController != null) {
            mBottomSheetController.removeObserver(mSheetObserver);
            mSheetObserver = null;
        }
        if (mComponentsCallbacks != null) {
            mContext.unregisterComponentCallbacks(mComponentsCallbacks);
            mComponentsCallbacks = null;
        }
        stopObservingCompositorViewInteractions();

        if (mKeyboardVisibilityListener != null) {
            mWindowAndroid
                    .getKeyboardDelegate()
                    .removeKeyboardVisibilityListener(mKeyboardVisibilityListener);
            mKeyboardVisibilityListener = null;
        }

        if (mObservedModalDialogManager != null) {
            if (mModalDialogManagerObserver != null) {
                mObservedModalDialogManager.removeObserver(mModalDialogManagerObserver);
            }
            mObservedModalDialogManager = null;
        }
        mModalDialogManagerObserver = null;

        if (mSheetContent != null) {
            mSheetContent.destroy();
            mSheetContent = null;
        }
        if (mViewBinder != null) {
            mViewBinder.destroy();
            mViewBinder = null;
        }
        mSheetEventsCallback = null;

        mIsShowingTabBottomSheet = false;
    }

    // Observer methods.
    private ComponentCallbacks buildComponentsCallback() {
        return new ComponentCallbacks() {
            @Override
            public void onConfigurationChanged(Configuration configuration) {
                Log.i(TAG, "onConfigurationChanged: isShowing = " + mIsShowingTabBottomSheet);
                if (mIsShowingTabBottomSheet) {
                    mExpectingLayoutChange = true;
                }
                mCoBrowseViews.setAllowFullscreenIme(
                        configuration.orientation == Configuration.ORIENTATION_LANDSCAPE);
            }

            @Override
            public void onLowMemory() {}
        };
    }

    private BottomSheetObserver buildBottomSheetObserver() {
        return new EmptyBottomSheetObserver() {
            private @SheetState int mLastStableState = SheetState.HIDDEN;

            @Override
            public void onSheetStateChanged(@SheetState int state, @StateChangeReason int reason) {
                if (mSheetContent == null
                        || mSheetEventsCallback == null
                        || !mIsShowingTabBottomSheet) return;
                mMediator.onSheetStateChanged(state);
                updateInactivePlaceholder();
                // We only send the opened notification when the sheet is not hidden and not in the
                // middle of a closing/hiding flow.
                if (state != SheetState.HIDDEN && !mBottomSheetController.isSheetHiding()) {
                    // The sheet is considered expanded if it's in HALF, FULL, or SCROLLING above
                    // peek.
                    boolean isExpanded = state != SheetState.PEEK;
                    mSheetEventsCallback.onBottomSheetOpened(isExpanded);
                }
                updateRoundingEdges();

                if (state == SheetState.HALF || state == SheetState.FULL) {
                    observeCompositorViewInteractions();
                    if (mContentView != null) {
                        mContentView.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
                    }
                } else {
                    stopObservingCompositorViewInteractions();
                }

                if (ChromeFeatureList.sTabBottomSheetResizeWebview.getValue()) {
                    mMediator.onSheetResizingStatusChanged(state == SheetState.SCROLLING);
                }

                if (state != SheetState.SCROLLING && state != SheetState.NONE) {
                    handleStableStateEntered(state, reason);
                }
            }

            @Override
            public void onContainerSizeChanged(int containerWidth, int containerHeight) {
                Log.i(
                        TAG,
                        "onContainerSizeChanged: width = "
                                + containerWidth
                                + ", height = "
                                + containerHeight
                                + ", expectingLayoutChange = "
                                + mExpectingLayoutChange);
                if (mSheetContent == null || !mIsShowingTabBottomSheet) {
                    return;
                }
                if (mExpectingLayoutChange) {
                    mBottomSheetController.collapseSheet(/* animate= */ true);
                    mExpectingLayoutChange = false;
                }
                if (ChromeFeatureList.sTabBottomSheetResizeWebview.getValue()) {
                    if (mInitialContainerSizeChanged) {
                        setToFlexibleHeight();
                    } else {
                        setToFixedHeightOrFallback();
                    }
                    mInitialContainerSizeChanged = true;
                } else {
                    setToFixedHeightOrFallback();
                }
            }

            @Override
            public void onContainerBottomMarginChanged(@Px int bottomMargin) {
                if (mSheetContent == null || !mIsShowingTabBottomSheet) {
                    return;
                }

                if (!ChromeFeatureList.sTabBottomSheetResizeWebview.getValue()) {
                    setToFixedHeightOrFallback();
                }
            }

            @Override
            public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
                if (mBottomSheetController.getSheetState() == SheetState.SCROLLING) {
                    mMediator.updateCrossFadeAlpha(offsetPx);
                }
            }

            // Called when the sheet content changes (e.g., when swapped out or hidden).
            @Override
            public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {
                if (mSheetEventsCallback == null) {
                    return;
                }
                if (newContent == mSheetContent) {
                    mIsShowingTabBottomSheet = true;
                } else {
                    if (mIsShowingTabBottomSheet) {
                        // When the sheet is immediately closed or content is swapped,
                        // onSheetStateChanged may not be called for HIDDEN. Record HIDDEN
                        // here to ensure full coverage. The deduplication guard in
                        // handleStableStateEntered prevents double-logging.
                        handleStableStateEntered(SheetState.HIDDEN, StateChangeReason.NONE);
                        mMediator.onSheetStateChanged(BottomSheetController.SheetState.HIDDEN);
                        mSheetEventsCallback.onBottomSheetClosed();
                        stopObservingCompositorViewInteractions();
                    }
                    mIsShowingTabBottomSheet = false;
                }
            }

            @Override
            public void onInsetAnimationEnd() {
                mCoBrowseViews.setIgnoreClearFocus(/* ignoreClearFocus= */ false);
            }

            private void handleStableStateEntered(
                    @SheetState int state, @StateChangeReason int reason) {
                if (mLastStableState == state) {
                    return;
                }

                @TabBottomSheetClientType int clientType = mCoBrowseViews.getClientType();

                // Record current state hit
                TabBottomSheetMetrics.recordStateHit(clientType, state);

                if (state == SheetState.PEEK && clientType == TabBottomSheetClientType.GLIC) {
                    GlicMetrics.recordShowPeekView();
                }

                if ((state == SheetState.HALF || state == SheetState.FULL)
                        && mLastStableState != SheetState.HALF
                        && mLastStableState != SheetState.FULL
                        && clientType == TabBottomSheetClientType.GLIC) {
                    GlicMetrics.recordShowBottomSheet();
                }

                // Record transition if between open stable states (PEEK, HALF, FULL)
                TabBottomSheetMetrics.recordTransition(clientType, mLastStableState, state);

                // Record state change reasons for PEEK and HIDDEN states
                TabBottomSheetMetrics.recordStateChangeReason(clientType, state, reason);

                mLastStableState = state;
            }
        };
    }

    private ModalDialogManagerObserver buildModalDialogManagerObserver() {
        return new ModalDialogManagerObserver() {
            @Override
            public void onDialogShown(View dialogView) {
                if (mObservedModalDialogManager != null
                        && mObservedModalDialogManager.getCurrentType() == ModalDialogType.TAB) {
                    collapseSheet();
                }
            }
        };
    }

    private KeyboardVisibilityListener buildKeyboardVisibilityListener() {
        return isShowing -> {
            mCoBrowseViews.setIgnoreClearFocus(isShowing);
            if (isShowing
                    && mIsShowingTabBottomSheet
                    && mContentView != null
                    && !mContentView.hasFocus()) {
                collapseSheet();
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
        if (keyboardDelegate == null) {
            return false;
        }
        return keyboardDelegate.isKeyboardShowing(mCoBrowseViews.getView());
    }

    private void setToFlexibleHeight() {
        if (isActivityInactive(mWindowAndroid)) return;
        mMediator.setToFlexibleHeight();
    }

    private @Px int getDesiredFixedHeight() {
        int viewportHeight = getVisibleViewportHeight();
        int desiredHeight = (int) (viewportHeight * getDefaultHeightRatio());

        int bottomMargin = mBottomSheetController.getContainerBottomMargin();

        // Prevent the bottom sheet from covering the bottom controls.
        if (desiredHeight + bottomMargin > viewportHeight) {
            desiredHeight = viewportHeight - bottomMargin;
        }

        return max(0, desiredHeight);
    }

    private void setToFixedHeightOrFallback() {
        if (isActivityInactive(mWindowAndroid)) return;
        @Px int fixedHeight = getDesiredFixedHeight();
        mMediator.setToFixedHeight(fixedHeight);

        // In the case the bottom sheet is unable to set to our desired fixed height, fallback to
        // use of flexible heights.
        if (ChromeFeatureList.sTabBottomSheetResizeWebview.getValue()
                && mBottomSheetController.getContainerHeight() != fixedHeight) {
            setToFlexibleHeight();
        }
    }

    private int getVisibleViewportHeight() {
        Window window = mWindowAndroid.getWindow();
        if (window == null) {
            return 0;
        }

        Rect visibleViewportRect = new Rect();
        View decorView = window.getDecorView();
        decorView.getWindowVisibleDisplayFrame(visibleViewportRect);

        @Px int decorHeight = window.getDecorView().getHeight();
        @Px int visibleHeight = Math.min(decorHeight, visibleViewportRect.height());
        Log.i(
                TAG,
                "getVisibleViewportHeight: decorHeight = "
                        + decorHeight
                        + ", visibleViewportRect.height() = "
                        + visibleViewportRect.height()
                        + ", resolved = "
                        + visibleHeight);
        return visibleHeight;
    }

    private float getDefaultHeightRatio() {
        Configuration configuration = mContext.getResources().getConfiguration();
        if (configuration.orientation == Configuration.ORIENTATION_LANDSCAPE) {
            return SMALL_SCREEN_HEIGHT_RATIO;
        }
        return isKeyboardShowing() ? SMALL_SCREEN_HEIGHT_RATIO : FULL_HEIGHT_RATIO;
    }

    private void updateRoundingEdges() {
        if (mOutlineProvider == null) return;

        boolean shouldRound = !mBottomSheetController.isFullWidth();
        if (mOutlineProvider.isTopEdgeRounded() == shouldRound) {
            return;
        }

        if (shouldRound) {
            mOutlineProvider.setRoundingEdges(true, true, true, false);
        } else {
            mOutlineProvider.setRoundingEdges(false, false, false, false);
        }

        if (mContentView != null) {
            mContentView.invalidateOutline();
        }
    }

    // Testing methods.
    RoundedCornerOutlineProvider getOutlineProviderForTesting() {
        return mOutlineProvider;
    }

    PropertyModel getModelForTesting() {
        return mModel;
    }

    boolean isSheetCurrentlyManagedForTesting() {
        return mIsShowingTabBottomSheet;
    }

    boolean isExpectingLayoutChangeForTesting() {
        return mExpectingLayoutChange;
    }

    GestureDetector.SimpleOnGestureListener getGestureListenerForTesting() {
        return mGestureListener;
    }

    @Nullable TabBottomSheetContent getSheetContentForTesting() {
        return mSheetContent;
    }

    private void updateInactivePlaceholder() {
        if (mSheetContent == null || mContentView == null) return;

        View placeholder = mContentView.findViewById(R.id.empty_placeholder_container);
        if (placeholder == null) return;

        @SheetState int state = mBottomSheetController.getSheetState();
        boolean isExpanded = state == SheetState.HALF || state == SheetState.FULL;
        boolean usePlaceholder = mSheetContent.usePlaceholder();
        boolean webContentsNull = mCoBrowseViews.getWebContentsSupplier().get() == null;

        boolean showPlaceholder = usePlaceholder && isExpanded && webContentsNull;
        placeholder.setVisibility(showPlaceholder ? View.VISIBLE : View.GONE);
    }
}
