// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Build;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.Interpolator;
import android.widget.FrameLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.SysUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.TabLoadStatus;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager.FullscreenListener;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ActionModeController.ActionBarDelegate;
import org.chromium.chrome.browser.toolbar.ViewShiftingActionBarDelegate;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.BrowserControlsState;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

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
public class BottomSheet extends FrameLayout
        implements BottomSheetSwipeDetector.SwipeableBottomSheet, NativePageHost {
    /** The different states that the bottom sheet can have. */
    @IntDef({SheetState.NONE, SheetState.HIDDEN, SheetState.PEEK, SheetState.HALF, SheetState.FULL,
            SheetState.SCROLLING})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SheetState {
        /**
         * NONE is for internal use only and indicates the sheet is not currently
         * transitioning between states.
         */
        int NONE = -1;
        // Values are used for indexing mStateRatios, should start from 0
        // and can't have gaps. Additionally order is important for these,
        // they go from smallest to largest.
        int HIDDEN = 0;
        int PEEK = 1;
        int HALF = 2;
        int FULL = 3;

        int SCROLLING = 4;
    }

    /** The different reasons that the sheet's state can change. */
    @IntDef({StateChangeReason.NONE, StateChangeReason.SWIPE, StateChangeReason.BACK_PRESS,
            StateChangeReason.TAP_SCRIM, StateChangeReason.NAVIGATION,
            StateChangeReason.COMPOSITED_UI, StateChangeReason.VR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface StateChangeReason {
        int NONE = 0;
        int SWIPE = 1;
        int BACK_PRESS = 2;
        int TAP_SCRIM = 3;
        int NAVIGATION = 4;
        int COMPOSITED_UI = 5;
        int VR = 6;
    }

    /** The different priorities that the sheet's content can have. */
    @IntDef({ContentPriority.HIGH, ContentPriority.LOW})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContentPriority {
        int HIGH = 0;
        int LOW = 1;
    }

    /**
     * The base duration of the settling animation of the sheet. 218 ms is a spec for material
     * design (this is the minimum time a user is guaranteed to pay attention to something).
     */
    public static final long BASE_ANIMATION_DURATION_MS = 218;

    /** The amount of time it takes to transition sheet content in or out. */
    private static final long TRANSITION_DURATION_MS = 150;

    /**
     * The fraction of the way to the next state the sheet must be swiped to animate there when
     * released. This is the value used when there are 3 active states. A smaller value here means
     * a smaller swipe is needed to move the sheet around.
     */
    private static final float THRESHOLD_TO_NEXT_STATE_3 = 0.5f;

    /** This is similar to {@link #THRESHOLD_TO_NEXT_STATE_3} but for 2 states instead of 3. */
    private static final float THRESHOLD_TO_NEXT_STATE_2 = 0.3f;

    /** The height ratio for the sheet in the SheetState.HALF state. */
    private static final float HALF_HEIGHT_RATIO = 0.65f;

    /** The fraction of the width of the screen that, when swiped, will cause the sheet to move. */
    private static final float SWIPE_ALLOWED_FRACTION = 0.2f;

    /**
     * The minimum swipe velocity (dp/ms) that should be considered as a user opening the bottom
     * sheet intentionally. This is specifically for the 'velocity' swipe logic.
     */
    private static final float SHEET_SWIPE_MIN_DP_PER_MS = 0.2f;

    /**
     * Information about the different scroll states of the sheet. Order is important for these,
     * they go from smallest to largest.
     */
    private final float[] mStateRatios = new float[4];

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

    /** The {@link BottomSheetMetrics} used to record user actions and histograms. */
    private final BottomSheetMetrics mMetrics;

    /** The view that contains the sheet. */
    private ViewGroup mSheetContainer;

    /** For detecting scroll and fling events on the bottom sheet. */
    private BottomSheetSwipeDetector mGestureDetector;

    /** The animator used to move the sheet to a fixed state when released by the user. */
    private ValueAnimator mSettleAnimator;

    /** The animator set responsible for swapping the bottom sheet content. */
    private AnimatorSet mContentSwapAnimatorSet;

    /** The height of the toolbar. */
    private float mToolbarHeight;

    /** The width of the view that contains the bottom sheet. */
    private float mContainerWidth;

    /** The height of the view that contains the bottom sheet. */
    private float mContainerHeight;

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
    protected TabModelSelector mTabModelSelector;

    /** The fullscreen manager for information about toolbar offsets. */
    private ChromeFullscreenManager mFullscreenManager;

    /** A handle to the content being shown by the sheet. */
    @Nullable
    protected BottomSheetContent mSheetContent;

    /** A handle to the find-in-page toolbar. */
    private View mFindInPageView;

    /** A handle to the FrameLayout that holds the content of the bottom sheet. */
    private TouchRestrictingFrameLayout mBottomSheetContentContainer;

    /**
     * The last ratio sent to observers of onTransitionPeekToHalf(). This is used to ensure the
     * final value sent to these observers is 1.0f.
     */
    private float mLastPeekToHalfRatioSent;

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

    /** The activity displaying the bottom sheet. */
    protected ChromeActivity mActivity;

    /** A delegate for when the action bar starts showing. */
    private ViewShiftingActionBarDelegate mActionBarDelegate;

    /** Whether {@link #destroy()} has been called. **/
    private boolean mIsDestroyed;

    /** The token used to enable browser controls persistence. */
    private int mPersistentControlsToken;

    /** Conversion ratio of dp to px. */
    private float mDpToPx;

    /** Whether or not scroll events are currently being blocked for the 'velocity' swipe logic. */
    private boolean mVelocityLogicBlockSwipe;

    /**
     * An interface defining content that can be displayed inside of the bottom sheet for Chrome
     * Home.
     */
    public interface BottomSheetContent {
        /**
         * Gets the {@link View} that holds the content to be displayed in the Chrome Home bottom
         * sheet.
         * @return The content view.
         */
        View getContentView();

        /**
         * Get the {@link View} that contains the toolbar specific to the content being
         * displayed. If null is returned, the omnibox is used.
         *
         * @return The toolbar view.
         */
        @Nullable
        View getToolbarView();

        /**
         * @return The vertical scroll offset of the content view.
         */
        int getVerticalScrollOffset();

        /**
         * Called to destroy the {@link BottomSheetContent} when it is no longer in use.
         */
        void destroy();

        /**
         * @return The priority of this content.
         */
        @ContentPriority
        int getPriority();

        /**
         * @return Whether swiping the sheet down hard enough will cause the sheet to be dismissed.
         */
        boolean swipeToDismissEnabled();

        /**
         * @return Whether the peek state is enabled.
         */
        boolean isPeekStateEnabled();

        /**
         * @return The resource id of the content description for the bottom sheet. This is
         *         generally the name of the feature/content that is showing. 'Swipe down to close.'
         *         will be automatically appended after the content description.
         */
        int getSheetContentDescriptionStringId();

        /**
         * @return The resource id of the string announced when the sheet is opened at half height.
         *         This is typically the name of your feature followed by 'opened at half height'.
         */
        int getSheetHalfHeightAccessibilityStringId();

        /**
         * @return The resource id of the string announced when the sheet is opened at full height.
         *         This is typically the name of your feature followed by 'opened at full height'.
         */
        int getSheetFullHeightAccessibilityStringId();

        /**
         * @return The resource id of the string announced when the sheet is closed. This is
         *         typically the name of your feature followed by 'closed'.
         */
        int getSheetClosedAccessibilityStringId();
    }

    /**
     * Returns whether the provided bottom sheet state is in one of the stable open or closed
     * states: {@link #SheetState.FULL}, {@link #SheetState.PEEK} or {@link #SheetState.HALF}
     * @param sheetState A {@link SheetState} to test.
     */
    public static boolean isStateStable(@SheetState int sheetState) {
        switch (sheetState) {
            case SheetState.HIDDEN:
            case SheetState.PEEK:
            case SheetState.HALF:
            case SheetState.FULL:
                return true;
            case SheetState.SCROLLING:
                return false;
            case SheetState.NONE: // Should never be tested, internal only value.
            default:
                assert false;
                return false;
        }
    }

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
        if (mActivity == null || isSheetOpen() || AccessibilityUtil.isAccessibilityEnabled()) {
            return true;
        }

        if (currentEvent.getActionMasked() == MotionEvent.ACTION_DOWN) {
            mVelocityLogicBlockSwipe = false;
        }

        float scrollDistanceDp = MathUtils.distance(initialEvent.getX(), initialEvent.getY(),
                                         currentEvent.getX(), currentEvent.getY())
                / mDpToPx;
        long timeDeltaMs = currentEvent.getEventTime() - initialEvent.getDownTime();

        String logicType = FeatureUtilities.getChromeHomeSwipeLogicType();

        // By default, the entire toolbar is swipable.
        float startX = mVisibleViewportRect.left;
        float endX = mDefaultToolbarView.getWidth() + mVisibleViewportRect.left;

        if (ChromeSwitches.CHROME_HOME_SWIPE_LOGIC_RESTRICT_AREA.equals(logicType)) {
            // Determine an area in the middle of the toolbar that is swipable. This will only
            // trigger if the expand button is disabled.
            float allowedSwipeWidth = mContainerWidth * SWIPE_ALLOWED_FRACTION;
            startX = mVisibleViewportRect.left + (mContainerWidth - allowedSwipeWidth) / 2;
            endX = startX + allowedSwipeWidth;
        } else if (ChromeSwitches.CHROME_HOME_SWIPE_LOGIC_VELOCITY.equals(logicType)
                || (ChromeFeatureList.isInitialized()
                           && ChromeFeatureList.isEnabled(
                                      ChromeFeatureList.CHROME_HOME_SWIPE_VELOCITY_FEATURE))) {
            if (mVelocityLogicBlockSwipe) return false;

            double dpPerMs = scrollDistanceDp / (double) timeDeltaMs;

            if (dpPerMs < SHEET_SWIPE_MIN_DP_PER_MS) {
                mVelocityLogicBlockSwipe = true;
                return false;
            }

            return true;
        }

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
                getResources().getDimensionPixelSize(R.dimen.chrome_home_min_full_half_distance);
        mToolbarShadowHeight =
                getResources().getDimensionPixelOffset(R.dimen.toolbar_shadow_height);

        mMetrics = new BottomSheetMetrics();
        addObserver(mMetrics);

        mGestureDetector = new BottomSheetSwipeDetector(context, this);
        mIsTouchEnabled = true;
    }

    /**
     * Called when the activity containing the {@link BottomSheet} is destroyed.
     */
    public void destroy() {
        mIsDestroyed = true;
        mIsTouchEnabled = false;
        mObservers.clear();
        endAnimations();
    }

    /**
     * Handle a back press event.
     *     - If the navigation stack is empty, the sheet will be opened to the half state.
     *         - If the tab switcher is visible, {@link ChromeActivity} will handle the event.
     *     - If the sheet is open it will be closed unless it was opened by a back press.
     * @return True if the sheet handled the back press.
     */
    public boolean handleBackPress() {
        if (isSheetOpen()) {
            setSheetState(SheetState.PEEK, true, StateChangeReason.BACK_PRESS);
            return true;
        }

        return false;
    }

    /**
     * Sets whether the {@link BottomSheet} and its children should react to touch events.
     */
    public void setTouchEnabled(boolean enabled) {
        mIsTouchEnabled = enabled;
    }

    /** Immediately end all animations and null the animators. */
    public void endAnimations() {
        if (mSettleAnimator != null) mSettleAnimator.end();
        mSettleAnimator = null;
        endTransitionAnimations();
    }

    /**
     * Immediately end the bottom sheet content transition animations and null the animator.
     */
    public void endTransitionAnimations() {
        if (mContentSwapAnimatorSet == null || !mContentSwapAnimatorSet.isRunning()) return;
        mContentSwapAnimatorSet.end();
        mContentSwapAnimatorSet = null;
    }

    /**
     * @return An action bar delegate that appropriately moves the sheet when the action bar is
     *         shown.
     */
    public ActionBarDelegate getActionBarDelegate() {
        return mActionBarDelegate;
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

        if (isToolbarAndroidViewHidden()) return false;

        mGestureDetector.onTouchEvent(e);

        return true;
    }

    /**
     * @return Whether or not the toolbar Android View is hidden due to being scrolled off-screen.
     */
    @VisibleForTesting
    public boolean isToolbarAndroidViewHidden() {
        return mFullscreenManager == null || mFullscreenManager.getBottomControlOffset() > 0
                || mToolbarHolder.getVisibility() != VISIBLE;
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int heightSize = MeasureSpec.getSize(heightMeasureSpec);
        assert heightSize != 0;
        int height = heightSize + mToolbarShadowHeight;
        super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY));
    }

    /**
     * Adds layout change listeners to the views that the bottom sheet depends on. Namely the
     * heights of the root view and control container are important as they are used in many of the
     * calculations in this class.
     * @param root The container of the bottom sheet.
     * @param activity The activity displaying the bottom sheet.
     */
    public void init(View root, ChromeActivity activity) {
        mTabModelSelector = activity.getTabModelSelector();
        mFullscreenManager = activity.getFullscreenManager();

        mToolbarHolder =
                (TouchRestrictingFrameLayout) findViewById(R.id.bottom_sheet_toolbar_container);
        mDefaultToolbarView = mToolbarHolder.findViewById(R.id.bottom_sheet_toolbar);
        mToolbarHeight =
                activity.getResources().getDimensionPixelSize(R.dimen.bottom_sheet_peek_height);

        mActivity = activity;
        mActionBarDelegate = new ViewShiftingActionBarDelegate(mActivity, this);

        getLayoutParams().height = ViewGroup.LayoutParams.MATCH_PARENT;

        mBottomSheetContentContainer =
                (TouchRestrictingFrameLayout) findViewById(R.id.bottom_sheet_content);
        mBottomSheetContentContainer.setBottomSheet(this);
        mBottomSheetContentContainer.setBackgroundColor(
                ApiCompatibilityUtils.getColor(getResources(), R.color.modern_primary_color));

        mDpToPx = mActivity.getResources().getDisplayMetrics().density;

        // Listen to height changes on the root.
        root.addOnLayoutChangeListener(new View.OnLayoutChangeListener() {
            private int mPreviousKeyboardHeight;

            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                // Compute the new height taking the keyboard into account.
                // TODO(mdjones): Share this logic with LocationBarLayout: crbug.com/725725.
                float previousWidth = mContainerWidth;
                float previousHeight = mContainerHeight;
                mContainerWidth = right - left;
                mContainerHeight = bottom - top;

                if (previousWidth != mContainerWidth || previousHeight != mContainerHeight) {
                    updateSheetStateRatios();
                }

                int heightMinusKeyboard = (int) mContainerHeight;
                int keyboardHeight = 0;

                // Reset mVisibleViewportRect regardless of sheet open state as it is used outside
                // of calculating the keyboard height.
                mActivity.getWindow().getDecorView().getWindowVisibleDisplayFrame(
                        mVisibleViewportRect);
                if (isSheetOpen()) {
                    int decorHeight = mActivity.getWindow().getDecorView().getHeight();
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
                    if (mGestureDetector.isScrolling() && mActivity.getWindowAndroid() != null) {
                        mActivity.getWindowAndroid().getKeyboardDelegate().hideKeyboard(
                                BottomSheet.this);
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

                if (!mGestureDetector.isScrolling()) {
                    cancelAnimation();

                    // This onLayoutChange() will be called after the user enters fullscreen video
                    // mode. Ensure the sheet state is reset to peek so that the sheet does not
                    // open over the fullscreen video. See crbug.com/740499.
                    if (mFullscreenManager != null
                            && mFullscreenManager.getPersistentFullscreenMode() && isSheetOpen()) {
                        setSheetState(SheetState.PEEK, false);
                    } else {
                        setSheetState(mCurrentState, false);
                    }
                }
            }
        });

        mFullscreenManager.addListener(new FullscreenListener() {
            @Override
            public void onToggleOverlayVideoMode(boolean enabled) {
                if (isSheetOpen()) setSheetState(SheetState.PEEK, false);
            }

            @Override
            public void onControlsOffsetChanged(
                    float topOffset, float bottomOffset, boolean needsAnimate) {
                if (getSheetState() == SheetState.HIDDEN) return;
                if (getCurrentOffsetPx() > getSheetHeightForState(SheetState.PEEK)) return;

                // Updating the offset will automatically account for the browser controls.
                setSheetOffsetFromBottom(getCurrentOffsetPx(), StateChangeReason.SWIPE);
            }

            @Override
            public void onContentOffsetChanged(float offset) {}

            @Override
            public void onBottomControlsHeightChanged(int bottomControlsHeight) {}
        });

        mSheetContainer = (ViewGroup) this.getParent();
        mSheetContainer.removeView(this);
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

        assert mTabModelSelector != null;

        int tabLoadStatus = TabLoadStatus.DEFAULT_PAGE_LOAD;

        if (getActiveTab() != null && getActiveTab().isIncognito() == incognito) {
            tabLoadStatus = getActiveTab().loadUrl(params);
        } else {
            // If no compatible tab is active behind the sheet, open a new one.
            mTabModelSelector.openNewTab(
                    params, TabModel.TabLaunchType.FROM_CHROME_UI, getActiveTab(), incognito);
        }

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
        return mTabModelSelector != null ? mTabModelSelector.getCurrentTab() : null;
    }

    @Override
    public boolean isVisible() {
        return mCurrentState != SheetState.PEEK;
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
    private @SheetState int getMinSwipableSheetState() {
        return swipeToDismissEnabled() || !mSheetContent.isPeekStateEnabled() ? SheetState.HIDDEN
                                                                              : SheetState.PEEK;
    }

    @Override
    public float getMaxOffsetPx() {
        return getFullRatio() * mContainerHeight;
    }

    /**
     * Show content in the bottom sheet's content area.
     * @param content The {@link BottomSheetContent} to show, or null if no content should be shown.
     */
    public void showContent(@Nullable final BottomSheetContent content) {
        // If an animation is already running, end it.
        if (mContentSwapAnimatorSet != null) mContentSwapAnimatorSet.end();

        // If the desired content is already showing, do nothing.
        if (mSheetContent == content) return;

        List<Animator> animators = new ArrayList<>();
        mContentSwapAnimatorSet = new AnimatorSet();
        mContentSwapAnimatorSet.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                onContentSwapAnimationEnd(content);
            }
        });

        // Add an animator for the toolbar transition if needed.
        View newToolbar = content != null && content.getToolbarView() != null
                ? content.getToolbarView()
                : mDefaultToolbarView;
        View oldToolbar = mSheetContent != null && mSheetContent.getToolbarView() != null
                ? mSheetContent.getToolbarView()
                : mDefaultToolbarView;
        if (newToolbar != oldToolbar) {
            // For the toolbar transition, make sure we don't detach the default toolbar view.
            Animator transitionAnimator = getViewTransitionAnimator(
                    newToolbar, oldToolbar, mToolbarHolder, mDefaultToolbarView != oldToolbar);
            if (transitionAnimator != null) animators.add(transitionAnimator);
        }

        // Add an animator for the content transition if needed.
        View oldContent = mSheetContent != null ? mSheetContent.getContentView() : null;
        if (content == null) {
            if (oldContent != null) mBottomSheetContentContainer.removeView(oldContent);
        } else {
            View contentView = content.getContentView();
            Animator transitionAnimator = getViewTransitionAnimator(
                    contentView, oldContent, mBottomSheetContentContainer, true);
            if (transitionAnimator != null) animators.add(transitionAnimator);
        }

        // Temporarily make the background of the toolbar holder a solid color so the transition
        // doesn't appear to show a hole in the toolbar.
        int colorId = R.color.modern_primary_color;
        if (!mIsSheetOpen) {
            // If the sheet is closed, the bottom sheet content container is invisible, so
            // background color is needed on the toolbar holder to prevent a blank rectangle from
            // appearing during the content transition.
            mToolbarHolder.setBackgroundColor(
                    ApiCompatibilityUtils.getColor(getResources(), colorId));
        }
        mBottomSheetContentContainer.setBackgroundColor(
                ApiCompatibilityUtils.getColor(getResources(), colorId));

        // Set color on the content view to compensate for a JellyBean bug (crbug.com/766237).
        if (content != null) {
            content.getContentView().setBackgroundColor(
                    ApiCompatibilityUtils.getColor(getResources(), colorId));
        }

        // Return early if there are no animators to run.
        if (animators.isEmpty()) {
            onContentSwapAnimationEnd(content);
            return;
        }

        mContentSwapAnimatorSet.playTogether(animators);
        mContentSwapAnimatorSet.start();

        // If the existing content is null or the tab switcher assets are showing, end the animation
        // immediately.
        if (mSheetContent == null || isInOverviewMode() || SysUtils.isLowEndDevice()) {
            mContentSwapAnimatorSet.end();
        }
    }

    /**
     * Called when the animation to swap BottomSheetContent ends.
     * @param content The BottomSheetContent showing at the end of the animation.
     */
    private void onContentSwapAnimationEnd(BottomSheetContent content) {
        if (mIsDestroyed) return;

        onSheetContentChanged(content);
        mContentSwapAnimatorSet = null;
    }

    /**
     * Creates a transition animation between two views. The old view is faded out completely
     * before the new view is faded in. There is an option to detach the old view or not.
     * @param newView The new view to transition to.
     * @param oldView The old view to transition from.
     * @param parent The parent for newView and oldView.
     * @param detachOldView Whether or not to detach the old view once faded out.
     * @return An animator that runs the specified animation or null if no animation should be run.
     */
    @Nullable
    private Animator getViewTransitionAnimator(final View newView, final View oldView,
            final ViewGroup parent, final boolean detachOldView) {
        if (newView == oldView) return null;

        AnimatorSet animatorSet = new AnimatorSet();
        List<Animator> animators = new ArrayList<>();

        newView.setVisibility(View.VISIBLE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                && !ValueAnimator.areAnimatorsEnabled()) {
            if (oldView != null) {
                // Post a runnable to remove the old view to prevent issues related to the keyboard
                // showing while swapping contents. See https://crbug.com/799252.
                post(() -> { swapViews(newView, oldView, parent, detachOldView); });
            } else {
                if (parent != newView.getParent()) parent.addView(newView);
            }

            newView.setAlpha(1);

            return null;
        }

        // Fade out the old view.
        if (oldView != null) {
            ValueAnimator fadeOutAnimator = ObjectAnimator.ofFloat(oldView, View.ALPHA, 0);
            fadeOutAnimator.setDuration(TRANSITION_DURATION_MS);
            fadeOutAnimator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    swapViews(newView, oldView, parent, detachOldView);
                }
            });
            animators.add(fadeOutAnimator);
        } else {
            // Normally the new view is added at the end of the fade-out animation of the old view,
            // if there is no old view, attach the new one immediately.
            if (parent != newView.getParent()) parent.addView(newView);
        }

        // Fade in the new view.
        newView.setAlpha(0);
        ValueAnimator fadeInAnimator = ObjectAnimator.ofFloat(newView, View.ALPHA, 1);
        fadeInAnimator.setDuration(TRANSITION_DURATION_MS);
        animators.add(fadeInAnimator);

        animatorSet.playSequentially(animators);

        return animatorSet;
    }

    /**
     * Removes the oldView (or sets it to invisible) and adds the new view to the specified parent.
     * @param newView The new view to transition to.
     * @param oldView The old view to transition from.
     * @param parent The parent for newView and oldView.
     * @param detachOldView Whether or not to detach the old view once faded out.
     */
    private void swapViews(final View newView, final View oldView, final ViewGroup parent,
            final boolean detachOldView) {
        if (detachOldView && oldView.getParent() != null) {
            parent.removeView(oldView);
        } else {
            oldView.setVisibility(View.INVISIBLE);
        }
        if (parent != newView.getParent()) parent.addView(newView);
    }

    /**
     * A notification that the sheet is exiting the peek state into one that shows content.
     * @param reason The reason the sheet was opened, if any.
     */
    private void onSheetOpened(@StateChangeReason int reason) {
        if (mIsSheetOpen) return;

        mIsSheetOpen = true;

        // Make sure the toolbar is visible before expanding the sheet.
        Tab tab = getActiveTab();
        if (isToolbarAndroidViewHidden() && tab != null) {
            tab.updateBrowserControlsState(BrowserControlsState.SHOWN, false);
        }

        mBottomSheetContentContainer.setVisibility(View.VISIBLE);

        // Browser controls should stay visible until the sheet is closed.
        mPersistentControlsToken =
                mFullscreenManager.getBrowserVisibilityDelegate().showControlsPersistent();

        dismissSelectedText();
        for (BottomSheetObserver o : mObservers) o.onSheetOpened(reason);
        mActivity.addViewObscuringAllTabs(this);
    }

    /**
     * A notification that the sheet has returned to the peeking state.
     * @param reason The {@link StateChangeReason} that the sheet was closed, if any.
     */
    private void onSheetClosed(@StateChangeReason int reason) {
        if (!mIsSheetOpen) return;
        mBottomSheetContentContainer.setVisibility(View.INVISIBLE);
        mIsSheetOpen = false;

        // Update the browser controls since they are permanently shown while the sheet is open.
        mFullscreenManager.getBrowserVisibilityDelegate().releasePersistentShowingToken(
                mPersistentControlsToken);

        for (BottomSheetObserver o : mObservers) o.onSheetClosed(reason);
        // If the sheet contents are cleared out before #onSheetClosed is called, do not try to
        // retrieve the accessibility string.
        if (getCurrentSheetContent() != null) {
            announceForAccessibility(getResources().getString(
                    getCurrentSheetContent().getSheetClosedAccessibilityStringId()));
        }
        clearFocus();
        mActivity.removeViewObscuringAllTabs(this);

        setFocusable(false);
        setFocusableInTouchMode(false);
        setContentDescription(null);
    }

    /**
     * Updates the bottom sheet's state ratios and adjusts the sheet's state if necessary.
     */
    private void updateSheetStateRatios() {
        if (mContainerHeight <= 0) return;

        // Though mStateRatios is a static constant, the peeking ratio is computed here because
        // the correct toolbar height and container height are not know until those views are
        // inflated. The other views are a specific DP distance from the top and bottom and are
        // also updated.
        mStateRatios[SheetState.HIDDEN] = 0;
        mStateRatios[SheetState.PEEK] = (mToolbarHeight + mToolbarShadowHeight) / mContainerHeight;
        mStateRatios[SheetState.HALF] = HALF_HEIGHT_RATIO;
        // The max height ratio will be greater than 1 to account for the toolbar shadow.
        mStateRatios[SheetState.FULL] =
                (mContainerHeight + mToolbarShadowHeight) / mContainerHeight;

        if (mCurrentState == SheetState.HALF && isSmallScreen()) {
            setSheetState(SheetState.FULL, false);
        }
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
        mSettleAnimator.setDuration(BASE_ANIMATION_DURATION_MS);
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

        if (targetState != SheetState.HIDDEN) {
            setInternalCurrentState(SheetState.SCROLLING, reason);
        }
        mSettleAnimator.start();
    }

    /**
     * @return Get the height in px that the peeking bar is offset due to the browser controls.
     */
    private float getOffsetFromBrowserControls() {
        float peekHeight = getPeekRatio() * mContainerHeight;
        return peekHeight * mFullscreenManager.getBrowserControlHiddenRatio();
    }

    /**
     * Sets the sheet's offset relative to the bottom of the screen.
     * @param offset The offset that the sheet should be.
     */
    private void setSheetOffsetFromBottom(float offset, @StateChangeReason int reason) {
        mCurrentOffsetPx = offset;

        // The browser controls offset is added here so that the sheet's toolbar behaves like the
        // browser controls do.
        float translationY = (mContainerHeight - mCurrentOffsetPx) + getOffsetFromBrowserControls();

        if (MathUtils.areFloatsEqual(translationY, getTranslationY())) return;

        setTranslationY(translationY);

        float hiddenHeight = getHiddenRatio() * mContainerHeight;
        if (mCurrentOffsetPx <= hiddenHeight && this.getParent() != null) {
            mSheetContainer.removeView(this);
        } else if (mCurrentOffsetPx > hiddenHeight && this.getParent() == null) {
            mSheetContainer.addView(this);
        }

        float peekHeight = getSheetHeightForState(SheetState.PEEK);
        boolean isAtPeekingHeight = MathUtils.areFloatsEqual(getCurrentOffsetPx(), peekHeight);
        if (isSheetOpen() && (getCurrentOffsetPx() < peekHeight || isAtPeekingHeight)) {
            onSheetClosed(reason);
        } else if (!isSheetOpen() && getCurrentOffsetPx() > peekHeight) {
            onSheetOpened(reason);
        }

        sendOffsetChangeEvents();
    }

    @Override
    public void setSheetOffset(float offset, boolean shouldAnimate) {
        cancelAnimation();

        if (shouldAnimate) {
            float velocityY = getCurrentOffsetPx() - offset;

            @BottomSheet.SheetState
            int targetState = getTargetSheetState(offset, -velocityY);

            setSheetState(targetState, true, BottomSheet.StateChangeReason.SWIPE);

            for (BottomSheetObserver o : mObservers) o.onSheetReleased();
        } else {
            setInternalCurrentState(
                    BottomSheet.SheetState.SCROLLING, BottomSheet.StateChangeReason.SWIPE);
            setSheetOffsetFromBottom(offset, BottomSheet.StateChangeReason.SWIPE);
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
        return mStateRatios[SheetState.HIDDEN];
    }

    /**
     * @return The ratio of the height of the screen that the peeking state is.
     */
    public float getPeekRatio() {
        return mStateRatios[SheetState.PEEK];
    }

    /**
     * @return The ratio of the height of the screen that the half expanded state is.
     */
    @VisibleForTesting
    float getHalfRatio() {
        return mStateRatios[SheetState.HALF];
    }

    /**
     * @return The ratio of the height of the screen that the fully expanded state is.
     */
    @VisibleForTesting
    float getFullRatio() {
        return mStateRatios[SheetState.FULL];
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

        float screenRatio = mContainerHeight > 0 ? offsetWithBrowserControls / mContainerHeight : 0;

        // This ratio is relative to the peek and full positions of the sheet.
        float hiddenFullRatio = MathUtils.clamp(
                (screenRatio - getHiddenRatio()) / (getFullRatio() - getHiddenRatio()), 0, 1);

        if (offsetWithBrowserControls < getSheetHeightForState(SheetState.HIDDEN)) {
            mLastOffsetRatioSent = 0;
        } else {
            mLastOffsetRatioSent =
                    MathUtils.areFloatsEqual(hiddenFullRatio, 0) ? 0 : hiddenFullRatio;
        }

        for (BottomSheetObserver o : mObservers) {
            o.onSheetOffsetChanged(mLastOffsetRatioSent, getCurrentOffsetPx());
        }

        if (MathUtils.areFloatsEqual(
                    offsetWithBrowserControls, getSheetHeightForState(SheetState.PEEK))) {
            for (BottomSheetObserver o : mObservers) o.onSheetFullyPeeked();
        }

        // This ratio is relative to the peek and half positions of the sheet.
        float peekHalfRatio = MathUtils.clamp(
                (screenRatio - getPeekRatio()) / (getHalfRatio() - getPeekRatio()), 0, 1);

        // If the ratio is close enough to zero, just set it to zero.
        if (MathUtils.areFloatsEqual(peekHalfRatio, 0f)) peekHalfRatio = 0f;

        if (peekHalfRatio != mLastPeekToHalfRatioSent
                && (mLastPeekToHalfRatioSent < 1f || peekHalfRatio < 1f)) {
            mLastPeekToHalfRatioSent = peekHalfRatio;
            for (BottomSheetObserver o : mObservers) {
                o.onTransitionPeekToHalf(peekHalfRatio);
            }
        }
    }

    /**
     * @see #setSheetState(int, boolean, int)
     */
    public void setSheetState(@SheetState int state, boolean animate) {
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
    public void setSheetState(
            @SheetState int state, boolean animate, @StateChangeReason int reason) {
        assert state != SheetState.SCROLLING && state != SheetState.NONE;

        // Half state is not valid on small screens.
        if (state == SheetState.HALF && isSmallScreen()) state = SheetState.FULL;

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
     *         returned. A target state will be set when the user releases the sheet from drag
     *         ({@link BottomSheetObserver#onSheetReleased()}) and has begun animation to the next
     *         state.
     */
    public int getTargetSheetState() {
        return mTargetState;
    }

    /**
     * @return The current state of the bottom sheet. If the sheet is animating, this will be the
     *         state the sheet is animating to.
     */
    @SheetState
    public int getSheetState() {
        return mCurrentState;
    }

    /** @return Whether the sheet is currently open. */
    public boolean isSheetOpen() {
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
            String swipeToClose = ". "
                    + getResources().getString(R.string.bottom_sheet_accessibility_description);
            setContentDescription(
                    getResources().getString(
                            getCurrentSheetContent().getSheetContentDescriptionStringId())
                    + swipeToClose);
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
    public boolean isRunningSettleAnimation() {
        return mSettleAnimator != null;
    }

    /**
     * @return Whether a content swap animation is in progress.
     */
    public boolean isRunningContentSwapAnimation() {
        return mContentSwapAnimatorSet != null && mContentSwapAnimatorSet.isRunning();
    }

    /**
     * @return The current sheet content, or null if there is no content.
     */
    @VisibleForTesting
    public @Nullable BottomSheetContent getCurrentSheetContent() {
        return mSheetContent;
    }

    /**
     * @return The {@link BottomSheetMetrics} used to record user actions and histograms.
     */
    public BottomSheetMetrics getBottomSheetMetrics() {
        return mMetrics;
    }

    /**
     * Gets the height of the bottom sheet based on a provided state.
     * @param state The state to get the height from.
     * @return The height of the sheet at the provided state.
     */
    public float getSheetHeightForState(@SheetState int state) {
        return mStateRatios[state] * mContainerHeight;
    }

    /**
     * Adds an observer to the bottom sheet.
     * @param observer The observer to add.
     */
    public void addObserver(BottomSheetObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes an observer to the bottom sheet.
     * @param observer The observer to remove.
     */
    public void removeObserver(BottomSheetObserver observer) {
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
        boolean shouldSkipHalfState = isMovingDownward || isSmallScreen();

        // First, find the two states that the sheet height is between.
        @SheetState
        int nextState = getMinSwipableSheetState();

        @SheetState
        int prevState = nextState;
        for (@SheetState int i = getMinSwipableSheetState(); i <= SheetState.FULL; i++) {
            if (i == SheetState.HALF && shouldSkipHalfState) continue;
            if (i == SheetState.PEEK && !mSheetContent.isPeekStateEnabled()) continue;
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

    public boolean isSmallScreen() {
        // A small screen is defined by there being less than 160dp between half and full states.
        float fullToHalfDiff = (getFullRatio() - getHalfRatio()) * mContainerHeight;
        return fullToHalfDiff < mMinHalfFullDistance;
    }

    /**
     * @return The default toolbar view.
     */
    @VisibleForTesting
    public @Nullable View getDefaultToolbarView() {
        return mDefaultToolbarView;
    }

    /**
     * @return The height of the toolbar holder.
     */
    public int getToolbarContainerHeight() {
        return mToolbarHolder != null ? mToolbarHolder.getHeight() : 0;
    }

    /**
     * @return The height of the toolbar shadow.
     */
    public int getToolbarShadowHeight() {
        return mToolbarShadowHeight;
    }

    /**
     * @return Whether or not the browser is in overview mode.
     */
    protected boolean isInOverviewMode() {
        return mActivity != null && mActivity.isInOverviewMode();
    }

    /**
     * Checks whether the sheet can be moved. It cannot be moved when the activity is in overview
     * mode, when "find in page" is visible, or when the toolbar is hidden.
     */
    protected boolean canMoveSheet() {
        if (mFindInPageView == null) mFindInPageView = findViewById(R.id.find_toolbar);
        boolean isFindInPageVisible =
                mFindInPageView != null && mFindInPageView.getVisibility() == View.VISIBLE;

        return !isToolbarAndroidViewHidden() && !isFindInPageVisible;
    }

    /**
     * Called when the sheet content has changed, to update dependent state and notify observers.
     * @param content The new sheet content, or null if the sheet has no content.
     */
    protected void onSheetContentChanged(@Nullable final BottomSheetContent content) {
        mSheetContent = content;
        for (BottomSheetObserver o : mObservers) {
            o.onSheetContentChanged(content);
        }
        mToolbarHolder.setBackgroundColor(Color.TRANSPARENT);
    }
}
