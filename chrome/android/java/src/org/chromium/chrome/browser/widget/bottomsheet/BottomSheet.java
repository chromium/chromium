// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Build;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.Interpolator;
import android.widget.FrameLayout;

import androidx.annotation.DimenRes;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.TabLoadStatus;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationDelegate;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController.SheetState;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;

/**
 * This class defines the bottom sheet that has multiple states and a persistently showing toolbar.
 * Namely, the states are:
 * - PEEK: Only the toolbar is visible at the bottom of the screen.
 * - HALF: The sheet is expanded to consume around half of the screen.
 * - FULL: The sheet is expanded to its full height.
 *
 * All the computation in this file is based off of the bottom of the screen instead of the top
 * for simplicity. This means that the bottom of the screen is 0 on the Y axis.
 */
class BottomSheet extends FrameLayout implements BottomSheetSwipeDetector.SwipeableBottomSheet,
                                                 NativePageHost, View.OnLayoutChangeListener {
    /**
     * The fraction of the way to the next state the sheet must be swiped to animate there when
     * released. This is the value used when there are 3 active states. A smaller value here means
     * a smaller swipe is needed to move the sheet around.
     */
    private static final float THRESHOLD_TO_NEXT_STATE_3 = 0.5f;

    /** This is similar to {@link #THRESHOLD_TO_NEXT_STATE_3} but for 2 states instead of 3. */
    private static final float THRESHOLD_TO_NEXT_STATE_2 = 0.3f;

    /** The height ratio for the sheet in the SheetState.HALF state. */
    private static final float HALF_HEIGHT_RATIO = 0.75f;

    /** The desired height of a content that has just been shown or whose height was invalidated. */
    private static final float HEIGHT_UNSPECIFIED = -1.0f;

    /** A flag to force the small screen state of the bottom sheet. */
    private static Boolean sIsSmallScreenForTesting;

    /** The interpolator that the height animator uses. */
    private final Interpolator mInterpolator = new DecelerateInterpolator(1.0f);

    /** The list of observers of this sheet. */
    private final ObserverList<BottomSheetObserver> mObservers = new ObserverList<>();

    /** The visible rect for the screen taking the keyboard into account. */
    private final Rect mVisibleViewportRect = new Rect();

    /** An out-array for use with getLocationInWindow to prevent constant allocations. */
    private final int[] mCachedLocation = new int[2];

    /** The minimum distance between half and full states to allow the half state. */
    private final float mMinHalfFullDistance;

    /** The height of the shadow that sits above the toolbar. */
    private final int mToolbarShadowHeight;

    /** The view that contains the sheet. */
    private ViewGroup mSheetContainer;

    /** For detecting scroll and fling events on the bottom sheet. */
    private BottomSheetSwipeDetector mGestureDetector;

    /** The animator used to move the sheet to a fixed state when released by the user. */
    private ValueAnimator mSettleAnimator;

    /** The width of the view that contains the bottom sheet. */
    private int mContainerWidth;

    /** The height of the view that contains the bottom sheet. */
    private int mContainerHeight;

    /** The desired height of the current content view. */
    private float mContentDesiredHeight = HEIGHT_UNSPECIFIED;

    /**
     * The current offset of the sheet from the bottom of the screen in px. This does not include
     * added offset from the scrolling of the browser controls which allows the sheet's toolbar to
     * show and hide in-sync with the top toolbar.
     */
    private float mCurrentOffsetPx;

    /** The current state that the sheet is in. */
    @SheetState
    private int mCurrentState = SheetState.HIDDEN;

    /** The target sheet state. This is the state that the sheet is currently moving to. */
    @SheetState
    private int mTargetState = SheetState.NONE;

    /** Used for getting the current tab. */
    protected Supplier<Tab> mTabSupplier;

    /** A handle to the content being shown by the sheet. */
    @Nullable
    protected BottomSheetContent mSheetContent;

    /** A handle to the find-in-page toolbar. */
    private View mFindInPageView;

    /** A handle to the FrameLayout that holds the content of the bottom sheet. */
    private TouchRestrictingFrameLayout mBottomSheetContentContainer;

    /**
     * The last offset ratio sent to observers of onSheetOffsetChanged(). This is used to ensure the
     * min and max values are provided at least once (0 and 1).
     */
    private float mLastOffsetRatioSent;

    /** The FrameLayout used to hold the bottom sheet toolbar. */
    private TouchRestrictingFrameLayout mToolbarHolder;

    /**
     * The default toolbar view. This is shown when the current bottom sheet content doesn't have
     * its own toolbar and when the bottom sheet is closed.
     */
    protected View mDefaultToolbarView;

    /** Whether the {@link BottomSheet} and its children should react to touch events. */
    private boolean mIsTouchEnabled;

    /** Whether the sheet is currently open. */
    private boolean mIsSheetOpen;

    /** Whether {@link #destroy()} has been called. **/
    private boolean mIsDestroyed;

    /** The ratio in the range [0, 1] that the browser controls are hidden. */
    private float mBrowserControlsHiddenRatio;

    @Override
    public boolean shouldGestureMoveSheet(MotionEvent initialEvent, MotionEvent currentEvent) {
        // If the sheet is scrolling off-screen or in the process of hiding, gestures should not
        // affect it.
        if (getCurrentOffsetPx() < getSheetHeightForState(SheetState.PEEK)
                || getOffsetFromBrowserControls() > 0) {
            return false;
        }

        // If the sheet is already open, the experiment is not enabled, or accessibility is enabled
        // there is no need to restrict the swipe area.
        if (isSheetOpen() || AccessibilityUtil.isAccessibilityEnabled()) {
            return true;
        }

        float startX = mVisibleViewportRect.left;
        float endX = getToolbarView().getWidth() + mVisibleViewportRect.left;
        return currentEvent.getRawX() > startX && currentEvent.getRawX() < endX;
    }

    /**
     * Constructor for inflation from XML.
     * @param context An Android context.
     * @param atts The XML attributes.
     */
    public BottomSheet(Context context, AttributeSet atts) {
        super(context, atts);

        mMinHalfFullDistance =
                getResources().getDimensionPixelSize(R.dimen.bottom_sheet_min_full_half_distance);
        mToolbarShadowHeight = getResources().getDimensionPixelOffset(getTopShadowResourceId());

        mGestureDetector = new BottomSheetSwipeDetector(context, this);
        mIsTouchEnabled = true;
    }

    /** @return The dimen describing the height of the shadow above the bottom sheet. */
    static @DimenRes int getTopShadowResourceId() {
        return R.dimen.bottom_sheet_toolbar_shadow_height;
    }

    /**
     * Called when the activity containing the {@link BottomSheet} is destroyed.
     */
    void destroy() {
        mIsDestroyed = true;
        mIsTouchEnabled = false;
        mObservers.clear();
        endAnimations();
    }

    /**
     * Sets whether the {@link BottomSheet} and its children should react to touch events.
     */
    public void setTouchEnabled(boolean enabled) {
        mIsTouchEnabled = enabled;
    }

    /** Immediately end all animations and null the animators. */
    void endAnimations() {
        if (mSettleAnimator != null) mSettleAnimator.end();
        mSettleAnimator = null;
    }

    /** @return Whether the sheet is in the process of hiding. */
    boolean isHiding() {
        return mSettleAnimator != null && mTargetState == SheetState.HIDDEN;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent e) {
        // If touch is disabled, act like a black hole and consume touch events without doing
        // anything with them.
        if (!mIsTouchEnabled) return true;

        if (!canMoveSheet()) return false;

        return mGestureDetector.onInterceptTouchEvent(e);
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        // If touch is disabled, act like a black hole and consume touch events without doing
        // anything with them.
        if (!mIsTouchEnabled) return true;

        mGestureDetector.onTouchEvent(e);

        return true;
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int heightSize = MeasureSpec.getSize(heightMeasureSpec);
        assert heightSize != 0;
        int height = heightSize + mToolbarShadowHeight;
        int mode = isFullHeightWrapContent() ? MeasureSpec.AT_MOST : MeasureSpec.EXACTLY;
        super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(height, mode));
    }

    /**
     * Adds layout change listeners to the views that the bottom sheet depends on. Namely the
     * heights of the root view and control container are important as they are used in many of the
     * calculations in this class.
     * @param tabProvider A means of accessing the active tab.
     * @param window Android window for getting insets.
     * @param keyboardDelegate Delegate for hiding the keyboard.
     */
    public void init(ActivityTabProvider tabProvider, Window window,
            KeyboardVisibilityDelegate keyboardDelegate) {
        View root = (View) getParent();
        mTabSupplier = tabProvider;

        mToolbarHolder =
                (TouchRestrictingFrameLayout) findViewById(R.id.bottom_sheet_toolbar_container);
        mToolbarHolder.setBackgroundResource(R.drawable.top_round);

        mDefaultToolbarView = mToolbarHolder.findViewById(R.id.bottom_sheet_toolbar);

        getLayoutParams().height = ViewGroup.LayoutParams.MATCH_PARENT;

        mBottomSheetContentContainer =
                (TouchRestrictingFrameLayout) findViewById(R.id.bottom_sheet_content);
        mBottomSheetContentContainer.setBottomSheet(this);
        mBottomSheetContentContainer.setBackgroundResource(R.drawable.top_round);

        mContainerWidth = root.getWidth();
        mContainerHeight = root.getHeight();

        // Listen to height changes on the root.
        root.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            private int mPreviousKeyboardHeight;

            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                // Compute the new height taking the keyboard into account.
                // TODO(mdjones): Share this logic with LocationBarLayout: crbug.com/725725.
                int previousWidth = mContainerWidth;
                int previousHeight = mContainerHeight;
                mContainerWidth = right - left;
                mContainerHeight = bottom - top;

                if (previousWidth != mContainerWidth || previousHeight != mContainerHeight) {
                    if (mCurrentState == SheetState.HALF && !isHalfStateEnabled()) {
                        setSheetState(SheetState.FULL, false);
                    }
                    invalidateContentDesiredHeight();
                }

                int heightMinusKeyboard = (int) mContainerHeight;
                int keyboardHeight = 0;

                // Reset mVisibleViewportRect regardless of sheet open state as it is used outside
                // of calculating the keyboard height.
                window.getDecorView().getWindowVisibleDisplayFrame(mVisibleViewportRect);
                if (isSheetOpen()) {
                    int decorHeight = window.getDecorView().getHeight();
                    heightMinusKeyboard = Math.min(decorHeight, mVisibleViewportRect.height());
                    keyboardHeight = (int) (mContainerHeight - heightMinusKeyboard);
                }

                if (keyboardHeight != mPreviousKeyboardHeight) {
                    // If the keyboard height changed, recompute the padding for the content area.
                    // This shrinks the content size while retaining the default background color
                    // where the keyboard is appearing. If the sheet is not showing, resize the
                    // sheet to its default state.
                    // Setting the padding is posted in a runnable for the sake of Android J.
                    // See crbug.com/751013.
                    final int finalPadding = keyboardHeight;
                    post(new Runnable() {
                        @Override
                        public void run() {
                            mBottomSheetContentContainer.setPadding(0, 0, 0, finalPadding);

                            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
                                // A layout on the toolbar holder is requested so that the toolbar
                                // doesn't disappear under certain scenarios on Android J.
                                // See crbug.com/751013.
                                mToolbarHolder.requestLayout();
                            }
                        }
                    });
                }

                if (previousHeight != mContainerHeight
                        || mPreviousKeyboardHeight != keyboardHeight) {
                    // If we are in the middle of a touch event stream (i.e. scrolling while
                    // keyboard is up) don't set the sheet state. Instead allow the gesture detector
                    // to position the sheet and make sure the keyboard hides.
                    if (mGestureDetector.isScrolling() && keyboardDelegate != null) {
                        keyboardDelegate.hideKeyboard(BottomSheet.this);
                    } else {
                        cancelAnimation();
                        setSheetState(mCurrentState, false);
                    }
                }

                mPreviousKeyboardHeight = keyboardHeight;
            }
        });

        // Listen to height changes on the toolbar.
        mToolbarHolder.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                // Make sure the size of the layout actually changed.
                if (bottom - top == oldBottom - oldTop && right - left == oldRight - oldLeft) {
                    return;
                }

                if (!mGestureDetector.isScrolling() && isRunningSettleAnimation()) return;

                setSheetState(mCurrentState, false);
            }
        });

        mSheetContainer = (ViewGroup) this.getParent();
        mSheetContainer.removeView(this);
    }

    /** @param ratio The current browser controls hidden ratio. */
    void setBrowserControlsHiddenRatio(float ratio) {
        mBrowserControlsHiddenRatio = ratio;

        if (getSheetState() == SheetState.HIDDEN) return;
        if (getCurrentOffsetPx() > getSheetHeightForState(SheetState.PEEK)) return;

        // Updating the offset will automatically account for the browser controls.
        setSheetOffsetFromBottom(getCurrentOffsetPx(), StateChangeReason.SWIPE);
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);

        // Trigger a relayout on window focus to correct any positioning issues when leaving Chrome
        // previously.  This is required as a layout is not triggered when coming back to Chrome
        // with the keyboard previously shown.
        if (hasWindowFocus) requestLayout();
    }

    @Override
    public int loadUrl(LoadUrlParams params, boolean incognito) {
        for (BottomSheetObserver o : mObservers) o.onLoadUrl(params.getUrl());

        int tabLoadStatus = TabLoadStatus.DEFAULT_PAGE_LOAD;

        if (getActiveTab() != null) tabLoadStatus = getActiveTab().loadUrl(params);

        return tabLoadStatus;
    }

    @Override
    public boolean isIncognito() {
        if (getActiveTab() == null) return false;
        return getActiveTab().isIncognito();
    }

    @Override
    public int getParentId() {
        return Tab.INVALID_TAB_ID;
    }

    @Override
    public Tab getActiveTab() {
        return mTabSupplier != null ? mTabSupplier.get() : null;
    }

    @Override
    public boolean isVisible() {
        return mCurrentState != SheetState.PEEK;
    }

    @Override
    public HistoryNavigationDelegate createHistoryNavigationDelegate() {
        assert false : "BottomSheet does not need HistoryNavigationDelegate";
        return null;
    }

    @Override
    public boolean isContentScrolledToTop() {
        return mSheetContent == null || mSheetContent.getVerticalScrollOffset() <= 0;
    }

    @Override
    public float getCurrentOffsetPx() {
        return mCurrentOffsetPx;
    }

    @Override
    public float getMinOffsetPx() {
        return (swipeToDismissEnabled() ? getHiddenRatio() : getPeekRatio()) * mContainerHeight;
    }

    @Override
    public boolean isTouchEventInToolbar(MotionEvent event) {
        mToolbarHolder.getLocationInWindow(mCachedLocation);
        // This check only tests for collision for the Y component since the sheet is the full width
        // of the screen. We only care if the touch event is above the bottom of the toolbar since
        // we won't receive an event if the touch is outside the sheet.
        return mCachedLocation[1] + mToolbarHolder.getHeight() > event.getRawY();
    }

    /**
     * @return Whether flinging down hard enough will close the sheet.
     */
    private boolean swipeToDismissEnabled() {
        return mSheetContent != null ? mSheetContent.swipeToDismissEnabled() : true;
    }

    /**
     * @return The minimum sheet state that the user can swipe to. i.e. flinging down will either
     *         close the sheet or peek it.
     */
    @SheetState
    int getMinSwipableSheetState() {
        return swipeToDismissEnabled() || !isPeekStateEnabled() ? SheetState.HIDDEN
                                                                : SheetState.PEEK;
    }

    /**
     * Get the state that the bottom sheet should open to with the provided content.
     * @return The minimum opened state for the current content.
     */
    @SheetState
    int getOpeningState() {
        if (mSheetContent == null) {
            return SheetState.HIDDEN;
        } else if (isPeekStateEnabled()) {
            return SheetState.PEEK;
        } else if (isHalfStateEnabled()) {
            return SheetState.HALF;
        }
        return SheetState.FULL;
    }

    @Override
    public float getMaxOffsetPx() {
        return getFullRatio() * mContainerHeight;
    }

    /**
     * Show content in the bottom sheet's content area.
     * @param content The {@link BottomSheetContent} to show, or null if no content should be shown.
     */
    @VisibleForTesting
    void showContent(@Nullable final BottomSheetContent content) {
        // If the desired content is already showing, do nothing.
        if (mSheetContent == content) return;

        // Remove this as listener from previous content layout and size changes.
        if (mSheetContent != null) {
            mSheetContent.setContentSizeListener(null);
            mSheetContent.getContentView().removeOnLayoutChangeListener(this);
        }

        swapViews(content != null ? content.getContentView() : null,
                mSheetContent != null ? mSheetContent.getContentView() : null,
                mBottomSheetContentContainer);

        View newToolbar = content != null ? content.getToolbarView() : null;
        swapViews(newToolbar, mSheetContent != null ? mSheetContent.getToolbarView() : null,
                mToolbarHolder);

        // We hide the default toolbar if the new content has its own.
        mDefaultToolbarView.setVisibility(newToolbar != null ? GONE : VISIBLE);

        onSheetContentChanged(content);
    }

    /**
     * Removes the oldView (or sets it to invisible) and adds the new view to the specified parent.
     * @param newView The new view to transition to.
     * @param oldView The old view to transition from.
     * @param parent The parent for newView and oldView.
     */
    private void swapViews(final View newView, final View oldView, final ViewGroup parent) {
        if (oldView != null && oldView.getParent() != null) parent.removeView(oldView);
        if (newView != null && parent != newView.getParent()) parent.addView(newView);
    }

    /**
     * A notification that the sheet is exiting the peek state into one that shows content.
     * @param reason The reason the sheet was opened, if any.
     */
    private void onSheetOpened(@StateChangeReason int reason) {
        if (mIsSheetOpen) return;

        mIsSheetOpen = true;

        dismissSelectedText();
        for (BottomSheetObserver o : mObservers) o.onSheetOpened(reason);
    }

    /**
     * A notification that the sheet has returned to the peeking state.
     * @param reason The {@link StateChangeReason} that the sheet was closed,
     *         if any.
     */
    private void onSheetClosed(@StateChangeReason int reason) {
        if (!mIsSheetOpen) return;
        mIsSheetOpen = false;

        for (BottomSheetObserver o : mObservers) o.onSheetClosed(reason);
        // If the sheet contents are cleared out before #onSheetClosed is called, do not try to
        // retrieve the accessibility string.
        if (getCurrentSheetContent() != null) {
            announceForAccessibility(getResources().getString(
                    getCurrentSheetContent().getSheetClosedAccessibilityStringId()));
        }
        clearFocus();

        setFocusable(false);
        setFocusableInTouchMode(false);
        setContentDescription(null);
    }

    /**
     * Cancels and nulls the height animation if it exists.
     */
    private void cancelAnimation() {
        if (mSettleAnimator == null) return;
        mSettleAnimator.cancel();
        mSettleAnimator = null;
    }

    /**
     * Creates the sheet's animation to a target state.
     * @param targetState The target state.
     * @param reason The reason the sheet started animation.
     */
    private void createSettleAnimation(
            @SheetState final int targetState, @StateChangeReason final int reason) {
        mTargetState = targetState;
        mSettleAnimator =
                ValueAnimator.ofFloat(getCurrentOffsetPx(), getSheetHeightForState(targetState));
        mSettleAnimator.setDuration(BottomSheetController.BASE_ANIMATION_DURATION_MS);
        mSettleAnimator.setInterpolator(mInterpolator);

        // When the animation is canceled or ends, reset the handle to null.
        mSettleAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animator) {
                if (mIsDestroyed) return;

                mSettleAnimator = null;
                setInternalCurrentState(targetState, reason);
                mTargetState = SheetState.NONE;
            }
        });

        mSettleAnimator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animator) {
                setSheetOffsetFromBottom((Float) animator.getAnimatedValue(), reason);
            }
        });

        setInternalCurrentState(SheetState.SCROLLING, reason);
        mSettleAnimator.start();
    }

    /**
     * @return Get the height in px that the peeking bar is offset due to the browser controls.
     */
    private float getOffsetFromBrowserControls() {
        if (mSheetContent == null || mSheetContent.hideOnScroll() || !isPeekStateEnabled()) {
            return 0;
        }

        return getPeekRatio() * mContainerHeight * mBrowserControlsHiddenRatio;
    }

    /**
     * Sets the sheet's offset relative to the bottom of the screen.
     * @param offset The offset that the sheet should be.
     */
    void setSheetOffsetFromBottom(float offset, @StateChangeReason int reason) {
        mCurrentOffsetPx = offset;

        // The browser controls offset is added here so that the sheet's toolbar behaves like the
        // browser controls do.
        float translationY = (mContainerHeight - mCurrentOffsetPx) + getOffsetFromBrowserControls();

        if (isSheetOpen() && MathUtils.areFloatsEqual(translationY, getTranslationY())) return;

        setTranslationY(translationY);

        float hiddenHeight = getHiddenRatio() * mContainerHeight;
        if (mCurrentOffsetPx <= hiddenHeight && this.getParent() != null) {
            mSheetContainer.removeView(this);
        } else if (mCurrentOffsetPx > hiddenHeight && this.getParent() == null) {
            mSheetContainer.addView(this);
        }

        // Do open/close computation based on the minimum allowed state by the sheet's content.
        // Note that when transitioning from hidden to peek, even dismissable sheets may want
        // to have a peek state.
        @SheetState
        int minSwipableState = getMinSwipableSheetState();
        if (isPeekStateEnabled() && !isSheetOpen() && mCurrentState != mTargetState) {
            minSwipableState = SheetState.PEEK;
        }

        float minScrollableHeight = getSheetHeightForState(minSwipableState);
        boolean isAtMinHeight = MathUtils.areFloatsEqual(getCurrentOffsetPx(), minScrollableHeight);
        boolean heightLessThanPeek = getCurrentOffsetPx() < minScrollableHeight;
        // Trigger the onSheetClosed event when the sheet is moving toward the hidden state if peek
        // is disabled. This should be fine since touch is disabled when the sheet's target is
        // hidden.
        boolean triggerCloseWithHidden = !isPeekStateEnabled() && mTargetState == SheetState.HIDDEN;

        if (isSheetOpen() && (heightLessThanPeek || isAtMinHeight || triggerCloseWithHidden)) {
            onSheetClosed(reason);
        } else if (!isSheetOpen() && mTargetState != SheetState.HIDDEN
                && getCurrentOffsetPx() > minScrollableHeight) {
            onSheetOpened(reason);
        }

        sendOffsetChangeEvents();
    }

    @Override
    public void setSheetOffset(float offset, boolean shouldAnimate) {
        cancelAnimation();
        if (mSheetContent == null) return;

        if (shouldAnimate) {
            float velocityY = getCurrentOffsetPx() - offset;

            @SheetState
            int targetState = getTargetSheetState(offset, -velocityY);

            setSheetState(targetState, true, StateChangeReason.SWIPE);
        } else {
            setInternalCurrentState(SheetState.SCROLLING, StateChangeReason.SWIPE);
            setSheetOffsetFromBottom(offset, StateChangeReason.SWIPE);
        }
    }

    /**
     * Deselects any text in the active tab's web contents and dismisses the text controls.
     */
    private void dismissSelectedText() {
        Tab activeTab = getActiveTab();
        if (activeTab == null) return;

        WebContents webContents = activeTab.getWebContents();
        if (webContents == null) return;
        SelectionPopupController.fromWebContents(webContents).clearSelection();
    }

    /**
     * This is the same as {@link #setSheetOffsetFromBottom(float, int)} but exclusively for
     * testing.
     * @param offset The offset to set the sheet to.
     */
    @VisibleForTesting
    public void setSheetOffsetFromBottomForTesting(float offset) {
        setSheetOffsetFromBottom(offset, StateChangeReason.NONE);
    }

    /**
     * @return The ratio of the height of the screen that the hidden state is.
     */
    @VisibleForTesting
    float getHiddenRatio() {
        return 0;
    }

    /** @return Whether the peeking state for the sheet's content is enabled. */
    boolean isPeekStateEnabled() {
        return mSheetContent != null && mSheetContent.getPeekHeight() != HeightMode.DISABLED;
    }

    /** @return Whether the half-height of the sheet is enabled. */
    private boolean isHalfStateEnabled() {
        if (mSheetContent == null) return false;

        // Half state is invalid on small screens, when wrapping content at full height, and when
        // explicitly disabled.
        return !isSmallScreen() && mSheetContent.getHalfHeightRatio() != HeightMode.DISABLED
                && mSheetContent.getFullHeightRatio() != HeightMode.WRAP_CONTENT;
    }

    /** @return Whether the height mode for the full state is WRAP_CONTENT. */
    private boolean isFullHeightWrapContent() {
        return mSheetContent != null
                && mSheetContent.getFullHeightRatio() == HeightMode.WRAP_CONTENT;
    }

    /**
     * @return The ratio of the height of the screen that the peeking state is.
     */
    public float getPeekRatio() {
        if (mContainerHeight <= 0 || !isPeekStateEnabled()) return 0;

        // If the content has a custom peek ratio set, use that instead of computing one.
        if (mSheetContent != null && mSheetContent.getPeekHeight() != HeightMode.DEFAULT) {
            assert mSheetContent.getPeekHeight()
                    != HeightMode.WRAP_CONTENT : "The peek mode can't wrap content.";
            float ratio = mSheetContent.getPeekHeight() / (float) mContainerHeight;
            assert ratio > 0 && ratio <= 1 : "Custom peek ratios must be in the range of (0, 1].";
            return ratio;
        }
        assert getToolbarView() != null : "Using default peek height requires a non-null toolbar";

        View toolbarView = getToolbarView();
        int toolbarHeight = toolbarView.getHeight();
        if (toolbarHeight == 0) {
            // If the toolbar is not laid out yet and has a fixed height layout parameter, we assume
            // that the toolbar will have this height in the future.
            ViewGroup.LayoutParams layoutParams = toolbarView.getLayoutParams();
            if (layoutParams != null) {
                if (layoutParams.height > 0) {
                    toolbarHeight = layoutParams.height;
                } else {
                    toolbarView.measure(
                            MeasureSpec.makeMeasureSpec(mContainerWidth, MeasureSpec.EXACTLY),
                            MeasureSpec.makeMeasureSpec(
                                    (int) mContainerHeight, MeasureSpec.AT_MOST));
                    toolbarHeight = toolbarView.getMeasuredHeight();
                }
            }
        }
        return (toolbarHeight + mToolbarShadowHeight) / (float) mContainerHeight;
    }

    private View getToolbarView() {
        return mSheetContent != null && mSheetContent.getToolbarView() != null
                ? mSheetContent.getToolbarView()
                : mDefaultToolbarView;
    }

    /**
     * @return The ratio of the height of the screen that the half expanded state is.
     */
    @VisibleForTesting
    float getHalfRatio() {
        if (mContainerHeight <= 0 || !isHalfStateEnabled()) return 0;

        float customHalfRatio = mSheetContent.getHalfHeightRatio();
        assert customHalfRatio
                != HeightMode.WRAP_CONTENT
            : "Half-height cannot be WRAP_CONTENT. This is only supported for full-height.";

        return customHalfRatio == HeightMode.DEFAULT ? HALF_HEIGHT_RATIO : customHalfRatio;
    }

    /**
     * @return The ratio of the height of the screen that the fully expanded state is.
     */
    @VisibleForTesting
    float getFullRatio() {
        if (mContainerHeight <= 0 || mSheetContent == null) return 0;

        float customFullRatio = mSheetContent.getFullHeightRatio();
        assert customFullRatio != HeightMode.DISABLED : "The full height cannot be DISABLED.";

        if (isFullHeightWrapContent()) {
            ensureContentDesiredHeightIsComputed();
            float heightPx =
                    Math.min(mContainerHeight, mContentDesiredHeight + mToolbarShadowHeight);
            return heightPx / mContainerHeight;
        }

        return customFullRatio == HeightMode.DEFAULT
                ? (mContainerHeight + mToolbarShadowHeight) / (float) mContainerHeight
                : customFullRatio;
    }

    /**
     * @return The height of the container that the bottom sheet exists in.
     */
    public float getSheetContainerHeight() {
        return mContainerHeight;
    }

    /**
     * Sends notifications if the sheet is transitioning from the peeking to half expanded state and
     * from the peeking to fully expanded state. The peek to half events are only sent when the
     * sheet is between the peeking and half states.
     */
    private void sendOffsetChangeEvents() {
        float offsetWithBrowserControls = getCurrentOffsetPx() - getOffsetFromBrowserControls();

        // Do not send events for states less than the hidden state unless 0 has not been sent.
        if (offsetWithBrowserControls <= getSheetHeightForState(SheetState.HIDDEN)
                && mLastOffsetRatioSent <= 0) {
            return;
        }

        float screenRatio =
                mContainerHeight > 0 ? offsetWithBrowserControls / (float) mContainerHeight : 0;

        // This ratio is relative to the peek and full positions of the sheet.
        float maxHiddenFullRatio = getFullRatio() - getHiddenRatio();
        float hiddenFullRatio = maxHiddenFullRatio == 0
                ? 0
                : MathUtils.clamp((screenRatio - getHiddenRatio()) / maxHiddenFullRatio, 0, 1);

        if (offsetWithBrowserControls < getSheetHeightForState(SheetState.HIDDEN)) {
            mLastOffsetRatioSent = 0;
        } else {
            mLastOffsetRatioSent =
                    MathUtils.areFloatsEqual(hiddenFullRatio, 0) ? 0 : hiddenFullRatio;
        }

        for (BottomSheetObserver o : mObservers) {
            o.onSheetOffsetChanged(mLastOffsetRatioSent, getCurrentOffsetPx());
        }

        if (isPeekStateEnabled()
                && MathUtils.areFloatsEqual(
                        offsetWithBrowserControls, getSheetHeightForState(SheetState.PEEK))) {
            for (BottomSheetObserver o : mObservers) o.onSheetFullyPeeked();
        }
    }

    /** @see #setSheetState(int, boolean, int) */
    void setSheetState(@SheetState int state, boolean animate) {
        setSheetState(state, animate, StateChangeReason.NONE);
    }

    /**
     * Moves the sheet to the provided state.
     * @param state The state to move the panel to. This cannot be SheetState.SCROLLING or
     *              SheetState.NONE.
     * @param animate If true, the sheet will animate to the provided state, otherwise it will
     *                move there instantly.
     * @param reason The reason the sheet state is changing. This can be specified to indicate to
     *               observers that a more specific event has occurred, otherwise
     *               STATE_CHANGE_REASON_NONE can be used.
     */
    void setSheetState(@SheetState int state, boolean animate, @StateChangeReason int reason) {
        assert state != SheetState.NONE;

        // Setting state to SCROLLING is not a valid operation. This can happen only when
        // we're already in the scrolling state. Make it no-op.
        if (state == SheetState.SCROLLING) {
            assert mCurrentState == SheetState.SCROLLING && isRunningSettleAnimation();
            return;
        }

        if (state == SheetState.HALF && !isHalfStateEnabled()) state = SheetState.FULL;

        mTargetState = state;

        cancelAnimation();

        if (animate && state != mCurrentState) {
            createSettleAnimation(state, reason);
        } else {
            setSheetOffsetFromBottom(getSheetHeightForState(state), reason);
            setInternalCurrentState(mTargetState, reason);
            mTargetState = SheetState.NONE;
        }
    }

    /**
     * @return The target state that the sheet is moving to during animation. If the sheet is
     *         stationary or a target state has not been determined, SheetState.NONE will be
     *         returned.
     */
    int getTargetSheetState() {
        return mTargetState;
    }

    /**
     * @return The current state of the bottom sheet. If the sheet is animating, this will be the
     *         state the sheet is animating to.
     */
    @SheetState
    int getSheetState() {
        return mCurrentState;
    }

    /** @return Whether the sheet is currently open. */
    boolean isSheetOpen() {
        return mIsSheetOpen;
    }

    /**
     * Set the current state of the bottom sheet. This is for internal use to notify observers of
     * state change events.
     * @param state The current state of the sheet.
     * @param reason The reason the state is changing if any.
     */
    private void setInternalCurrentState(@SheetState int state, @StateChangeReason int reason) {
        if (state == mCurrentState) return;

        // TODO(mdjones): This shouldn't be able to happen, but does occasionally during layout.
        //                Fix the race condition that is making this happen.
        if (state == SheetState.NONE) {
            setSheetState(getTargetSheetState(getCurrentOffsetPx(), 0), false);
            return;
        }

        mCurrentState = state;

        if (mCurrentState == SheetState.HALF || mCurrentState == SheetState.FULL) {
            int resId = mCurrentState == SheetState.FULL
                    ? getCurrentSheetContent().getSheetFullHeightAccessibilityStringId()
                    : getCurrentSheetContent().getSheetHalfHeightAccessibilityStringId();
            announceForAccessibility(getResources().getString(resId));

            // TalkBack will announce the content description if it has changed, so wait to set the
            // content description until after announcing full/half height.
            setFocusable(true);
            setFocusableInTouchMode(true);
            String contentDescription = getResources().getString(
                    getCurrentSheetContent().getSheetContentDescriptionStringId());

            if (getCurrentSheetContent().swipeToDismissEnabled()) {
                contentDescription += ". "
                        + getResources().getString(R.string.bottom_sheet_accessibility_description);
            }

            setContentDescription(contentDescription);
            if (getFocusedChild() == null) requestFocus();
        }

        for (BottomSheetObserver o : mObservers) {
            o.onSheetStateChanged(mCurrentState);
        }
    }

    /**
     * If the animation to settle the sheet in one of its states is running.
     * @return True if the animation is running.
     */
    private boolean isRunningSettleAnimation() {
        return mSettleAnimator != null;
    }

    /** @return The current sheet content, or null if there is no content. */
    @Nullable
    BottomSheetContent getCurrentSheetContent() {
        return mSheetContent;
    }

    /**
     * Gets the height of the bottom sheet based on a provided state.
     * @param state The state to get the height from.
     * @return The height of the sheet at the provided state.
     */
    private float getSheetHeightForState(@SheetState int state) {
        if (isFullHeightWrapContent() && state == SheetState.FULL) {
            ensureContentDesiredHeightIsComputed();
            return mContentDesiredHeight + mToolbarShadowHeight;
        }

        return getRatioForState(state) * mContainerHeight;
    }

    private void ensureContentDesiredHeightIsComputed() {
        if (mContentDesiredHeight != HEIGHT_UNSPECIFIED) {
            return;
        }

        mSheetContent.getContentView().measure(
                MeasureSpec.makeMeasureSpec(mContainerWidth, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(mContainerHeight, MeasureSpec.AT_MOST));
        mContentDesiredHeight = mSheetContent.getContentView().getMeasuredHeight();
    }

    private float getRatioForState(int state) {
        switch (state) {
            case SheetState.HIDDEN:
                return getHiddenRatio();
            case SheetState.PEEK:
                return getPeekRatio();
            case SheetState.HALF:
                return getHalfRatio();
            case SheetState.FULL:
                return getFullRatio();
        }

        throw new IllegalArgumentException("Invalid state: " + state);
    }

    /**
     * Adds an observer to the bottom sheet.
     * @param observer The observer to add.
     */
    void addObserver(BottomSheetObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes an observer to the bottom sheet.
     * @param observer The observer to remove.
     */
    void removeObserver(BottomSheetObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Gets the target state of the sheet based on the sheet's height and velocity.
     * @param sheetHeight The current height of the sheet.
     * @param yVelocity The current Y velocity of the sheet. This is only used for determining the
     *                  scroll or fling direction. If this value is positive, the movement is from
     *                  bottom to top.
     * @return The target state of the bottom sheet.
     */
    @SheetState
    private int getTargetSheetState(float sheetHeight, float yVelocity) {
        if (sheetHeight <= getMinOffsetPx()) return getMinSwipableSheetState();
        if (sheetHeight >= getMaxOffsetPx()) return SheetState.FULL;

        boolean isMovingDownward = yVelocity < 0;
        boolean shouldSkipHalfState = isMovingDownward || !isHalfStateEnabled();

        // First, find the two states that the sheet height is between.
        @SheetState
        int nextState = getMinSwipableSheetState();

        @SheetState
        int prevState = nextState;
        for (@SheetState int i = getMinSwipableSheetState(); i <= SheetState.FULL; i++) {
            if (i == SheetState.HALF && shouldSkipHalfState) continue;
            if (i == SheetState.PEEK && !isPeekStateEnabled()) continue;
            prevState = nextState;
            nextState = i;
            // The values in PanelState are ascending, they should be kept that way in order for
            // this to work.
            if (sheetHeight >= getSheetHeightForState(prevState)
                    && sheetHeight < getSheetHeightForState(nextState)) {
                break;
            }
        }

        // If the desired height is close enough to a certain state, depending on the direction of
        // the velocity, move to that state.
        float lowerBound = getSheetHeightForState(prevState);
        float distance = getSheetHeightForState(nextState) - lowerBound;

        float threshold =
                shouldSkipHalfState ? THRESHOLD_TO_NEXT_STATE_2 : THRESHOLD_TO_NEXT_STATE_3;
        float thresholdToNextState = yVelocity < 0.0f ? 1 - threshold : threshold;

        if ((sheetHeight - lowerBound) / distance > thresholdToNextState) {
            return nextState;
        }
        return prevState;
    }

    @VisibleForTesting
    public static void setSmallScreenForTesting(boolean isSmallScreen) {
        sIsSmallScreenForTesting = isSmallScreen;
    }

    public boolean isSmallScreen() {
        if (sIsSmallScreenForTesting != null) return sIsSmallScreenForTesting;

        // A small screen is defined by there being less than 160dp between half and full states.
        float fullHeightRatio =
                (mContainerHeight + mToolbarShadowHeight) / (float) mContainerHeight;
        float fullToHalfDiff = (fullHeightRatio - HALF_HEIGHT_RATIO) * mContainerHeight;
        return fullToHalfDiff < mMinHalfFullDistance;
    }

    /**
     * @return The height of the toolbar shadow.
     */
    public int getToolbarShadowHeight() {
        return mToolbarShadowHeight;
    }

    /**
     * Checks whether the sheet can be moved. It cannot be moved when the activity is in overview
     * mode, when "find in page" is visible, when the toolbar is in the animation to hide, or when
     * the toolbar is hidden.
     */
    protected boolean canMoveSheet() {
        if (mFindInPageView == null) mFindInPageView = findViewById(R.id.find_toolbar);
        boolean isFindInPageVisible =
                mFindInPageView != null && mFindInPageView.getVisibility() == View.VISIBLE;

        return !isFindInPageVisible && mTargetState != SheetState.HIDDEN;
    }

    /**
     * Called when the sheet content has changed, to update dependent state and notify observers.
     * @param content The new sheet content, or null if the sheet has no content.
     */
    protected void onSheetContentChanged(@Nullable final BottomSheetContent content) {
        mSheetContent = content;

        if (isFullHeightWrapContent()) {
            // Listen for layout/size changes.
            if (!content.setContentSizeListener(this::onContentSizeChanged)) {
                content.getContentView().addOnLayoutChangeListener(this);
            }

            invalidateContentDesiredHeight();
            ensureContentIsWrapped(/* animate= */ true);

            // HALF state is forbidden when wrapping the content.
            if (mCurrentState == SheetState.HALF) {
                setSheetState(SheetState.FULL, /* animate= */ true);
            }
        }

        for (BottomSheetObserver o : mObservers) {
            o.onSheetContentChanged(content);
        }
        mToolbarHolder.setBackgroundColor(Color.TRANSPARENT);
    }

    /**
     * Called when the sheet content layout changed.
     */
    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        invalidateContentDesiredHeight();
        ensureContentIsWrapped(/* animate= */ true);
    }

    /**
     * Called when the sheet content size changed.
     */
    private void onContentSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        mContentDesiredHeight = height;
        ensureContentIsWrapped(/* animate= */ false);
    }

    private void ensureContentIsWrapped(boolean animate) {
        if (mCurrentState == SheetState.HIDDEN || mCurrentState == SheetState.PEEK) return;

        // The SCROLLING state is used when animating the sheet height or when the user is swiping
        // the sheet. If it is the latter, we should not change the sheet height.
        if (!isRunningSettleAnimation() && mCurrentState == SheetState.SCROLLING) return;
        setSheetState(mCurrentState, animate);
    }

    private void invalidateContentDesiredHeight() {
        mContentDesiredHeight = HEIGHT_UNSPECIFIED;
    }
}
