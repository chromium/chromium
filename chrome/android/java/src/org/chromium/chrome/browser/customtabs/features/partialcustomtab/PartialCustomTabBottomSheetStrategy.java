// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET;
import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET_MAXIMIZED;
import static androidx.browser.customtabs.CustomTabsCallback.ACTIVITY_LAYOUT_STATE_FULL_SCREEN;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.app.Activity;
import android.content.res.Configuration;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Handler;
import android.view.GestureDetector;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.animation.AccelerateInterpolator;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.Px;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsCallback;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import androidx.swiperefreshlayout.widget.CircularProgressDrawable;

import org.chromium.base.MathUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.ContentGestureListener.GestureState;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.components.browser_ui.widget.TouchEventProvider;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.util.ColorUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * CustomTabHeightStrategy for Partial Custom Tab. An instance of this class should be owned by the
 * CustomTabActivity. Refer to {@link
 * https://docs.google.com/document/d/1YuFXHai2JECqAPE_HgamcKid3VTR05GAvJcyb4jaL6o/edit?usp=sharing}
 * for detailed inner workings and issues addressed along the way.
 */
public class PartialCustomTabBottomSheetStrategy extends PartialCustomTabBaseStrategy
        implements ConfigurationChangedObserver,
                ValueAnimator.AnimatorUpdateListener,
                PartialCustomTabHandleStrategy.DragEventCallback,
                TouchEventObserver {
    @VisibleForTesting static final long SPINNER_TIMEOUT_MS = 500;
    @VisibleForTesting static final int BOTTOM_SHEET_MAX_WIDTH_DP_LANDSCAPE = 900;

    /** Minimal height the bottom sheet CCT should show is half of the display height. */
    private static final float MINIMAL_HEIGHT_RATIO = 0.5f;

    /**
     * The maximum height we can snap to is under experiment, we have two branches, 90% of the
     * display height and 100% of the display height. This ratio is used to calculate the 90% of the
     * display height.
     */
    private static final float EXTRA_HEIGHT_RATIO = 0.1f;

    private static final int SPINNER_FADEIN_DURATION_MS = 100;
    private static final int SPINNER_FADEOUT_DURATION_MS = 400;
    private static final int NAVBAR_BUTTON_HIDE_SHOW_DELAY_MS = 150;

    @IntDef({
        HeightStatus.TOP,
        HeightStatus.INITIAL_HEIGHT,
        HeightStatus.TRANSITION,
        HeightStatus.CLOSE
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface HeightStatus {
        int TOP = 0;
        int INITIAL_HEIGHT = 1;
        int TRANSITION = 2;
        int CLOSE = 3;
    }

    private final AnimatorListener mSpinnerFadeoutAnimatorListener;
    private final @Px int mUnclampedInitialHeight;
    private final boolean mIsFixedHeight;
    private final Supplier<TouchEventProvider> mTouchEventProvider;
    private final Supplier<Tab> mTab;

    private CustomTabToolbar.HandleStrategy mHandleStrategy;
    private GestureDetector mGestureDetector;
    private ContentGestureListener mGestureHandler;

    private TabAnimator mTabAnimator;

    private @HeightStatus int mStatus = HeightStatus.INITIAL_HEIGHT;

    private ImageView mSpinnerView;
    private LinearLayout mNavbar;
    private CircularProgressDrawable mSpinner;
    private Runnable mSoftKeyboardRunnable;
    private boolean mStopShowingSpinner;
    private boolean mRestoreAfterFindPage;
    private boolean mContentScrollMayResizeTab;

    // Y offset when a dragging gesture/animation starts.
    private int mMoveStartY;
    private float mOffsetY;

    // Used to initialize the coordinator view (R.id.coordinator) to full-height at the beginning.
    // This is a workaround to an issue of the host app briefly flashing when the tab is resized.
    private boolean mInitFirstHeight;

    public PartialCustomTabBottomSheetStrategy(
            Activity activity,
            BrowserServicesIntentDataProvider intentData,
            Supplier<TouchEventProvider> touchEventProvider,
            Supplier<Tab> tab,
            OnResizedCallback onResizedCallback,
            OnActivityLayoutCallback onActivityLayoutCallback,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            FullscreenManager fullscreenManager,
            boolean isTablet,
            boolean startMaximized,
            PartialCustomTabHandleStrategyFactory handleStrategyFactory) {
        super(
                activity,
                intentData,
                onResizedCallback,
                onActivityLayoutCallback,
                fullscreenManager,
                isTablet,
                handleStrategyFactory);

        mTouchEventProvider = touchEventProvider;
        mTab = tab;

        int animTime = mActivity.getResources().getInteger(android.R.integer.config_mediumAnimTime);
        mTabAnimator = new TabAnimator(this, animTime, this::onMoveEnd);
        lifecycleDispatcher.register(this);
        if (startMaximized) mStatus = HeightStatus.TOP;

        mSpinnerFadeoutAnimatorListener =
                new AnimatorListener() {
                    @Override
                    public void onAnimationStart(Animator animator) {}

                    @Override
                    public void onAnimationRepeat(Animator animator) {}

                    @Override
                    public void onAnimationEnd(Animator animator) {
                        mSpinner.stop();
                        mSpinnerView.setVisibility(View.GONE);
                    }

                    @Override
                    public void onAnimationCancel(Animator animator) {}
                };

        mPositionUpdater = mVersionCompat::updatePosition;

        mUnclampedInitialHeight = intentData.getInitialActivityHeight();
        mIsFixedHeight = intentData.isPartialCustomTabFixedHeight();
        mContentScrollMayResizeTab = intentData.contentScrollMayResizeTab();
        if (mContentScrollMayResizeTab) {
            mGestureHandler = new ContentGestureListener(mTab, this, this::isFullyExpanded);
            mGestureDetector =
                    new GestureDetector(
                            activity, mGestureHandler, ThreadUtils.getUiThreadHandler());
        }
    }

    @Override
    public @PartialCustomTabType int getStrategyType() {
        return PartialCustomTabType.BOTTOM_SHEET;
    }

    @Override
    public @StringRes int getTypeStringId() {
        return R.string.accessibility_partial_custom_tab_bottom_sheet;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent e) {
        assert mContentScrollMayResizeTab;
        mGestureDetector.onTouchEvent(e);
        return mGestureHandler.getState() == GestureState.DRAG_TAB;
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        assert mContentScrollMayResizeTab;
        if (mGestureHandler.getState() == GestureState.SCROLL_CONTENT) {
            mTab.get().getContentView().onTouchEvent(e);
            // Do not return here even if motion events are targeted to the content view.
            // We keep feeding the gesture detector so it can monitor the state changes
            // and can switch the target to PCCT when necessary.
        }

        int action = e.getActionMasked();

        // The down event is interpreted above in onInterceptTouchEvent, it does not need to be
        // interpreted a second time.
        if (action != MotionEvent.ACTION_DOWN) mGestureDetector.onTouchEvent(e);

        // If the user is scrolling and the event is a cancel or up action, update scroll state and
        // return. Fling should have already cleared the gesture state. The following is for
        // the non-fling release.
        if (mGestureHandler.getState() != GestureState.NONE
                && (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL)) {
            mGestureHandler.doNonFlingRelease();
        }
        return true;
    }

    private boolean isFullyExpanded() {
        WindowManager.LayoutParams attrs = mActivity.getWindow().getAttributes();
        return attrs.y <= getFullyExpandedY();
    }

    @Override
    public void onShowSoftInput(Runnable softKeyboardRunnable) {
        // Expands to full height to avoid the tab being hidden by the soft keyboard.
        // Necessary only if we're at the initial height status.
        if (isFullHeight() || mStatus != HeightStatus.INITIAL_HEIGHT) {
            softKeyboardRunnable.run();
            return;
        }
        mSoftKeyboardRunnable = softKeyboardRunnable;
        animateTabTo(HeightStatus.TOP, /* autoResize= */ true);
    }

    /**
     * Animates the tab to the position associated with the target status.
     * @param targetStatus Target height status to animate to.
     * @param autoResize {@code true} if the animation starts not by user gesture dragging
     *        the tab but by other UI actions such as soft keyboard display/find-in-page command.
     */
    private void animateTabTo(int targetStatus, boolean autoResize) {
        // Tab cannot be animated to top/initial when running in full height.
        assert !(isFullHeight()
                && (targetStatus == HeightStatus.TOP
                        || targetStatus == HeightStatus.INITIAL_HEIGHT));

        Window window = mActivity.getWindow();
        window.addFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
        WindowManager.LayoutParams attrs = window.getAttributes();

        int end;
        switch (targetStatus) {
            case HeightStatus.TOP:
                // Make the window full height if it is not already. Otherwise background app
                // can flash briefly while expanding.
                if (attrs.height < mDisplayHeight - mNavbarHeight) {
                    attrs.height = mDisplayHeight - mNavbarHeight;
                    window.setAttributes(attrs);
                }
                end = getFullyExpandedY();
                break;
            case HeightStatus.INITIAL_HEIGHT:
                end = initialY();
                break;
            case HeightStatus.CLOSE:
                end = mDisplayHeight;
                if (isFullHeight()) {
                    attrs.y = getFullyExpandedY();
                    window.setAttributes(attrs);
                }
                break;
            default:
                end = 0;
                assert false : "Target status should be either top or initial height";
        }

        mStatus = HeightStatus.TRANSITION;
        if (autoResize) mMoveStartY = window.getAttributes().y;
        mTabAnimator.start(attrs.y, end, targetStatus, autoResize);
    }

    @Override
    public void onPostInflationStartup() {
        super.onPostInflationStartup();

        // Bottom-sheet can start in fullscreen mode. Remove the top margin.
        if (isFullscreen()) setTopMargins(0, 0);
    }

    @Override
    protected void updatePosition() {
        if (isFullscreen() || mActivity.findViewById(android.R.id.content) == null) return;

        initializeHeight();
        positionAtWidth(mVersionCompat.getDisplayWidth());
        if (shouldDrawDividerLine()) {
            resetCoordinatorLayoutInsets();
            drawDividerLine();
        }
        updateShadowOffset();
        maybeInvokeResizeCallback();
        if (!isFixedHeight()) mRestoreAfterFindPage = false;
    }

    private int initialY() {
        return mDisplayHeight - initialHeightInPortraitMode();
    }

    private int initialHeightInPortraitMode() {
        assert !isFullHeight() : "initialHeightInPortraitMode() is used in portrait mode only";
        return MathUtils.clamp(
                mUnclampedInitialHeight,
                mDisplayHeight - mStatusbarHeight,
                (int) (mDisplayHeight * MINIMAL_HEIGHT_RATIO));
    }

    @Override
    public void onToolbarInitialized(
            View coordinatorView, CustomTabToolbar toolbar, @Px int toolbarCornerRadius) {
        super.onToolbarInitialized(coordinatorView, toolbar, toolbarCornerRadius);

        mHandleStrategy =
                new PartialCustomTabHandleStrategy(
                        mActivity, this::isFullHeight, () -> mStatus, this);
        toolbar.setHandleStrategy(mHandleStrategy);
        toolbar.setMinimizeButtonEnabled(false);
        CustomTabDragBar dragBar = mActivity.findViewById(R.id.drag_bar);
        dragBar.setHandleStrategy(mHandleStrategy);
        View dragHandle = mActivity.findViewById(R.id.drag_handle);
        dragHandle.setOnClickListener(v -> onDragBarTapped());

        if (mContentScrollMayResizeTab) {
            mTouchEventProvider.get().addTouchEventObserver(this);
        }
        updateDragBarVisibility();
    }

    private void onDragBarTapped() {
        if (mStatus == HeightStatus.TRANSITION) {
            mStatus = mTabAnimator.getTargetStatus();
            mTabAnimator.cancel();
        }
        int newStatus =
                switch (mStatus) {
                    case HeightStatus.INITIAL_HEIGHT -> HeightStatus.TOP;
                    case HeightStatus.TOP -> HeightStatus.INITIAL_HEIGHT;
                    default -> {
                        assert false : "Invalid height status: " + mStatus;
                        yield HeightStatus.INITIAL_HEIGHT;
                    }
                };
        animateTabTo(newStatus, false);
    }

    // ConfigurationChangedObserver implementation.
    @Override
    public void onConfigurationChanged(Configuration newConfig) {}

    // ValueAnimator.AnimatorUpdateListener implementation.
    @Override
    public void onAnimationUpdate(ValueAnimator valueAnimator) {
        int value = (int) valueAnimator.getAnimatedValue();
        updateWindowPos(value, false);
    }

    @Override
    protected void cleanupImeStateCallback() {
        if (mVersionCompat.setImeStateCallback(null)) {
            mStatus = HeightStatus.INITIAL_HEIGHT;
        }
    }

    @Override
    protected @CustomTabsCallback.ActivityLayoutState int getActivityLayoutState() {
        if (isFullscreen()) {
            return ACTIVITY_LAYOUT_STATE_FULL_SCREEN;
        } else if (isMaximized()) {
            return ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET_MAXIMIZED;
        } else {
            return ACTIVITY_LAYOUT_STATE_BOTTOM_SHEET;
        }
    }

    @Override
    protected void adjustCornerRadius(GradientDrawable d, int radius) {
        View handleView = mActivity.findViewById(R.id.custom_tabs_handle_view);
        View dragBar = handleView.findViewById(R.id.drag_bar);
        ViewGroup.LayoutParams dragBarLayoutParams = dragBar.getLayoutParams();
        dragBarLayoutParams.height = mCachedHandleHeight;
        dragBar.setLayoutParams(dragBarLayoutParams);

        d.mutate();
        d.setCornerRadii(new float[] {radius, radius, radius, radius, 0, 0, 0, 0});
    }

    @Override
    public void setScrimFraction(float scrimFraction) {
        @ColorInt int scrimColor = mActivity.getColor(R.color.default_scrim_color);

        // Drag handle view is not part of CoordinatorLayout. As the root UI scrim changes, the
        // handle view color needs updating to match it. This is a better way than running PCCT's
        // own scrim coordinator since it can apply shape-aware scrim to the handle view that has
        // the rounded corner.
        getDragBarBackground()
                .setColor(ColorUtils.overlayColor(mToolbarColor, scrimColor, scrimFraction));

        ImageView handle = mActivity.findViewById(R.id.drag_handle);
        @ColorInt int handleColor = mActivity.getColor(R.color.drag_handlebar_color_baseline);
        if (scrimFraction > 0.f) {
            handle.setColorFilter(ColorUtils.overlayColor(handleColor, scrimColor, scrimFraction));
        } else {
            handle.clearColorFilter();
        }
    }

    @Override
    public void onFindToolbarShown() {
        if (mIsTablet) return;
        int findToolbarBackground = mActivity.getColor(R.color.find_in_page_background_color);
        getDragBarBackground().setColor(findToolbarBackground);

        if (isFullHeight()) return;

        // We get zero search results if the content view is entirely hidden by the soft keyboard,
        // which can happen if the tab is at initial height. Expand it.
        if (mStatus == HeightStatus.INITIAL_HEIGHT) {
            animateTabTo(HeightStatus.TOP, /* autoResize= */ true);
            mRestoreAfterFindPage = true;
        }
    }

    @Override
    public void onFindToolbarHidden() {
        if (mIsTablet) return;
        getDragBarBackground().setColor(mToolbarColor);

        if (isFullHeight()) return;
        if (mRestoreAfterFindPage) {
            animateTabTo(HeightStatus.INITIAL_HEIGHT, /* autoResize= */ true);
            mRestoreAfterFindPage = false;
        }
    }

    @Override
    protected void initializeHeight() {
        super.initializeHeight();

        int maxExpandedY = getFullyExpandedY();
        @Px int height = 0;

        if (!isFullHeight()) {
            if (mStatus == HeightStatus.INITIAL_HEIGHT) {
                height = initialHeightInPortraitMode();
            } else if (mStatus == HeightStatus.TOP) {
                height = mDisplayHeight - maxExpandedY;
            }
        }

        WindowManager.LayoutParams attrs = mActivity.getWindow().getAttributes();
        if (attrs.height == height) return;

        // To avoid the bottom navigation bar area flickering when starting dragging, position
        // web contents area right above the navigation bar so the two won't overlap. The
        // navigation area now just shows whatever is underneath: 1) loading view/web contents
        // while dragging 2) host app's navigation bar when at rest.
        positionAtHeight(height);
        if (!mInitFirstHeight) {
            setCoordinatorLayoutHeight(mDisplayHeight);
            mInitFirstHeight = true;
            new Handler().post(() -> setCoordinatorLayoutHeight(MATCH_PARENT));
        }
        updateDragBarVisibility();
    }

    private void positionAtWidth(int width) {
        WindowManager.LayoutParams attrs = mActivity.getWindow().getAttributes();
        if (isFullHeight() || isFullscreen()) {
            attrs.width = MATCH_PARENT;
            attrs.x = 0;
        } else {
            int x = 0;
            if (isLandscapeMaxWidth(width)) {
                float density = mActivity.getResources().getDisplayMetrics().density;
                width = (int) (BOTTOM_SHEET_MAX_WIDTH_DP_LANDSCAPE * density);
                x = (mDisplayWidth - width) / 2 + mVersionCompat.getXOffset();
            }
            attrs.width = width;
            attrs.x = x;
        }

        mActivity.getWindow().setAttributes(attrs);
    }

    private void positionAtHeight(int height) {
        WindowManager.LayoutParams attrs = mActivity.getWindow().getAttributes();
        if (isFullHeight() || isFullscreen()) {
            attrs.height = MATCH_PARENT;
            attrs.y = 0;
        } else {
            attrs.height = height - mNavbarHeight;
            attrs.y = mDisplayHeight - attrs.height - mNavbarHeight;
        }

        attrs.gravity = Gravity.TOP | Gravity.START;

        mActivity.getWindow().setAttributes(attrs);
    }

    private boolean isMaxWidthLandscapeBottomSheet() {
        int displayWidth = mVersionCompat.getDisplayWidth();
        return isLandscapeMaxWidth(displayWidth);
    }

    private void updateDragBarVisibility() {
        updateDragBarVisibility(
                /*dragHandlebarVisibility*/ isFixedHeight() ? View.GONE : View.VISIBLE);
    }

    @Override
    protected void setTopMargins(int shadowOffset, int handleOffset) {
        View handleView = mActivity.findViewById(R.id.custom_tabs_handle_view);
        boolean isMaxWidthLandscapeBottomSheet = isMaxWidthLandscapeBottomSheet();

        float maxWidthBottomSheetEv =
                mActivity.getResources().getDimensionPixelSize(R.dimen.default_elevation_2);
        float regBottomSheetEv =
                mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_elevation);
        float elevation = isMaxWidthLandscapeBottomSheet ? maxWidthBottomSheetEv : regBottomSheetEv;

        ViewGroup coordinatorLayout = mActivity.findViewById(R.id.coordinator);
        coordinatorLayout.setElevation(elevation);
        if (handleView != null) {
            handleView.setElevation(elevation);
        }

        int sideOffset =
                shouldDrawDividerLine() || isFullscreen()
                        ? 0
                        : mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.custom_tabs_shadow_offset);
        int sideMargin = isMaxWidthLandscapeBottomSheet ? sideOffset : 0;
        if (handleView != null) {
            ViewGroup.MarginLayoutParams lp =
                    (ViewGroup.MarginLayoutParams) handleView.getLayoutParams();
            lp.setMargins(sideMargin, shadowOffset, sideMargin, 0);
        }

        // Make enough room for the handle View.
        ViewGroup.MarginLayoutParams mlp =
                (ViewGroup.MarginLayoutParams) mToolbarCoordinator.getLayoutParams();
        mlp.setMargins(sideMargin, handleOffset, sideMargin, 0);
    }

    @Override
    protected int getHandleHeight() {
        return isFullHeight() ? 0 : mCachedHandleHeight;
    }

    @Override
    protected boolean shouldHaveNoShadowOffset() {
        return mStatus == HeightStatus.TOP
                || mActivity.getWindow().getAttributes().y <= getFullyExpandedY();
    }

    @Override
    protected boolean isFullHeight() {
        return MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity);
    }

    private boolean isLandscape() {
        return mOrientation == Configuration.ORIENTATION_LANDSCAPE;
    }

    private boolean isFixedHeight() {
        return mIsFixedHeight;
    }

    private void updateWindowPos(@Px int y, boolean userGesture) {
        // Do not allow the Window to go above the minimum threshold capped by the status
        // bar and (optionally) the 90%-height adjustment.
        int topY = getFullyExpandedY();
        y = MathUtils.clamp(y, topY, mDisplayHeight);
        Window window = mActivity.getWindow();
        WindowManager.LayoutParams attrs = window.getAttributes();
        if (attrs.y == y) return;

        // If the tab is not resizable then dragging it higher than the initial height will not be
        // allowed. The tab can still be dragged down in order to be closed.
        if (isFixedHeight() && userGesture && y < initialY()) return;

        attrs.y = y;
        window.setAttributes(attrs);
        if (mFinishRunnable != null) return;

        // Starting dragging from INITIAL_HEIGHT state, we can hide the spinner if the tab:
        // 1) reaches full height
        // 2) is dragged below the initial height
        if (mStatus == HeightStatus.INITIAL_HEIGHT
                && (y <= topY || y > initialY())
                && isSpinnerVisible()) {
            hideSpinnerView();
            if (y <= topY) {
                // Once reaching full-height, tab can hide the spinner permanently till
                // the finger is lifted. Keep it hidden.
                mStopShowingSpinner = true;
                return;
            }
        }
        // Show the spinner lazily, only when the tab is dragged _up_, which requires showing
        // more area than initial state.
        if (!mStopShowingSpinner
                && mStatus != HeightStatus.TRANSITION
                && !isSpinnerVisible()
                && y < mMoveStartY) {
            showSpinnerView();
            // We do not have to keep the spinner till the end of dragging action, since it doesn't
            // have the flickering issue at the end. Keeping it visible up to 500ms is sufficient to
            // hide the initial glitch that can briefly expose the host app screen at the beginning.
            new Handler()
                    .postDelayed(
                            () -> {
                                hideSpinnerView();
                                mStopShowingSpinner = true;
                            },
                            SPINNER_TIMEOUT_MS);
        }
        if (isSpinnerVisible()) {
            centerSpinnerVertically((ViewGroup.LayoutParams) mSpinnerView.getLayoutParams());
        }
    }

    private boolean isSpinnerVisible() {
        return mSpinnerView != null && mSpinnerView.getVisibility() == View.VISIBLE;
    }

    private void onMoveStart() {
        Window window = mActivity.getWindow();
        window.addFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
        WindowManager.LayoutParams attrs = window.getAttributes();
        attrs.height = mDisplayHeight;
        window.setAttributes(attrs);
        showNavbarButtons(false);
    }

    private void onMoveEnd() {
        mStatus = mTabAnimator.getTargetStatus();
        if (mFinishRunnable != null) {
            mFinishRunnable.run();
            return;
        }

        hideSpinnerView();
        showNavbarButtons(true);
        finishResizing(mStatus);
        updateShadowOffset();
        if (shouldDrawDividerLine()) drawDividerLine();
        if (mSoftKeyboardRunnable != null) {
            mSoftKeyboardRunnable.run();
            mSoftKeyboardRunnable = null;
            mVersionCompat.setImeStateCallback(this::onImeStateChanged);
        }

        if (AccessibilityState.isScreenReaderEnabled()) {
            int textId =
                    mStatus == HeightStatus.TOP
                            ? R.string.accessibility_custom_tab_expanded
                            : R.string.accessibility_custom_tab_collapsed;
            getCoordinatorLayout()
                    .announceForAccessibility(mActivity.getResources().getString(textId));
        }
    }

    @VisibleForTesting
    void onImeStateChanged(boolean imeVisible) {
        if (!imeVisible) {
            // Soft keyboard was hidden. Restore the tab to initial height state.
            mVersionCompat.setImeStateCallback(null);
            animateTabTo(HeightStatus.INITIAL_HEIGHT, /* autoResize= */ true);
        }
    }

    private void finishResizing(int targetStatus) {
        Window window = mActivity.getWindow();
        window.clearFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS);
        positionAtHeight(mDisplayHeight - window.getAttributes().y);
        maybeInvokeResizeCallback();
        mStatus = targetStatus;
        if (mFinishRunnable != null) {
            Runnable oldFinishRunnable = mFinishRunnable;
            mFinishRunnable = null;
            handleCloseAnimation(oldFinishRunnable);
        }
    }

    private void hideSpinnerView() {
        // TODO(crbug.com/40226472): Look into observing a view resize event to ensure the fade
        // animation can always cover the transition artifact.
        if (isSpinnerVisible()) {
            mSpinnerView
                    .animate()
                    .alpha(0f)
                    .setDuration(SPINNER_FADEOUT_DURATION_MS)
                    .setListener(mSpinnerFadeoutAnimatorListener);
        }
    }

    private void showSpinnerView() {
        if (mSpinnerView != null) {
            centerSpinnerVertically((ViewGroup.LayoutParams) mSpinnerView.getLayoutParams());
        } else {
            mSpinnerView = new ImageView(mActivity);
            mSpinnerView.setElevation(
                    mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_elevation));
            mSpinnerView.setBackgroundColor(mActivity.getColor(R.color.window_background_color));

            // Toolbar should not be hidden by spinner screen.
            ViewGroup.MarginLayoutParams lp = new ViewGroup.MarginLayoutParams(MATCH_PARENT, 0);
            int topMargin = mToolbarView.getHeight();
            lp.setMargins(0, topMargin, 0, 0);

            mSpinner = new CircularProgressDrawable(mActivity);
            mSpinner.setStyle(CircularProgressDrawable.LARGE);
            mSpinnerView.setImageDrawable(mSpinner);
            mSpinnerView.setScaleType(ImageView.ScaleType.CENTER);
            int[] colorList = new int[1];
            colorList[0] = mActivity.getColor(R.color.default_bg_color_blue);
            mSpinner.setColorSchemeColors(colorList);
            centerSpinnerVertically(lp);
        }
        // Spinner view is added to CoordinatorLayoutForPointer (not R.id.content) to obscure
        // the flickering at the beginning of dragging action.
        if (mSpinnerView.getParent() == null) getCoordinatorLayout().addView(mSpinnerView);
        mSpinnerView.clearAnimation();
        mSpinnerView.setAlpha(0.f);
        mSpinnerView.setVisibility(View.VISIBLE);
        mSpinnerView.animate().alpha(1.f).setDuration(SPINNER_FADEIN_DURATION_MS).setListener(null);
        mSpinner.start();
    }

    private void centerSpinnerVertically(ViewGroup.LayoutParams lp) {
        int toolbarHeight = mToolbarView.getHeight();
        int cctHeight = mDisplayHeight - mActivity.getWindow().getAttributes().y - toolbarHeight;
        lp.height = cctHeight;
        mSpinnerView.setLayoutParams(lp);
    }

    private void changeVisibilityNavbarButtons(boolean show) {
        View decorView = mActivity.getWindow().getDecorView();
        WindowInsetsControllerCompat controller =
                WindowCompat.getInsetsController(mActivity.getWindow(), decorView);
        if (show) {
            controller.show(WindowInsetsCompat.Type.navigationBars());
        } else {
            // Can we remove the slow fade-out animation?
            controller.hide(WindowInsetsCompat.Type.navigationBars());
        }
    }

    private void showNavbarButtons(boolean show) {
        // Resizing while the navbar buttons are visible, at times, flashes the host app.
        // http://crbug/1360425 fixed this for when the navbar buttons are hidden, so taking
        // advantage of that fix by hiding for a bit the navigation buttons, during the time the
        // flashing usually occurs. The navbar buttons need to be visible while resizing so that
        // the immersive mode confirmation dialog is not displayed, as fixed with
        // http://crbug/1360453
        // TODO: http://crbug/1373984 for follow-up on long term solution for fixing host app
        // flashing issues.
        if (!show) {
            changeVisibilityNavbarButtons(false);
            new Handler()
                    .postDelayed(
                            () -> {
                                changeVisibilityNavbarButtons(true);
                            },
                            NAVBAR_BUTTON_HIDE_SHOW_DELAY_MS);
        }
    }

    @VisibleForTesting
    @Px
    int getFullyExpandedY() {
        return mStatusbarHeight;
    }

    @Override
    protected boolean isMaximized() {
        return mStatus == HeightStatus.TOP;
    }

    // CustomTabHeightStrategy implementation

    @Override
    public boolean changeBackgroundColorForResizing() {
        // Need to return true to keep the transparent background we set in the init step.
        return true;
    }

    @Override
    public boolean handleCloseAnimation(Runnable finishRunnable) {
        if (!super.handleCloseAnimation(finishRunnable)) return false;

        mVersionCompat.setImeStateCallback(null);

        // Tapping the close button while in transition state should be ignored.
        // Delay it till the height settles in to either top/initial state, where the animation
        // begins when it detects the presence of |mFinishRunnable|.
        if (mStatus == HeightStatus.TRANSITION) return false;

        animateTabTo(HeightStatus.CLOSE, /* autoResize= */ true);
        return true;
    }

    // DragEventCallback implementation

    @Override
    public void onDragStart(int y) {
        onMoveStart();
        Window window = mActivity.getWindow();
        mMoveStartY = window.getAttributes().y;
        mOffsetY = mMoveStartY - y;
        mStopShowingSpinner = false;
    }

    @Override
    public void onDragMove(int y) {
        updateWindowPos((int) (y + mOffsetY), true);
    }

    @Override
    public boolean onDragEnd(int flingDistance) {
        int currentY = mActivity.getWindow().getAttributes().y;
        int finalY = currentY + flingDistance;
        int topY = getFullyExpandedY();
        int initialY = initialY();
        int bottomY = mDisplayHeight - mNavbarHeight;

        if (finalY < initialY) { // Move up
            boolean toTop = Math.abs(topY - finalY) < Math.abs(finalY - initialY);
            animateTabTo(
                    toTop && !isFixedHeight() ? HeightStatus.TOP : HeightStatus.INITIAL_HEIGHT,
                    /* autoResize= */ false);
            return true;
        } else { // Move down
            // Prevents skipping initial state when swiping from the top.
            if (mStatus == HeightStatus.TOP) finalY = Math.min(initialY, finalY);

            if (Math.abs(initialY - finalY) < Math.abs(finalY - bottomY)) {
                animateTabTo(HeightStatus.INITIAL_HEIGHT, /* autoResize= */ false);
                return true;
            }
        }
        // Tab is being closed. Animation is initiated in |handleCloseAnimation()|.
        return false;
    }

    // FullscreenManager.Observer implementation

    @Override
    public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
        // Enter fullscreen i.e. (x, y, height, width) = (0, 0, MATCH_PARENT, MATCH_PARENT)
        assert isFullscreen() : "Fullscreen mode should be on";
        positionAtHeight(/* height= */ 0); // |height| is not used
        positionAtWidth(/* width= */ 0); // |width| is not used
        setTopMargins(0, 0);
        maybeInvokeResizeCallback();
    }

    @Override
    public void onExitFullscreen(Tab tab) {
        // Ignore the notification coming before toolbar/post-inflation initialization, which
        // can happen when devices gets rotated while fullscreen video is playing.
        if (mHandleStrategy == null) return;

        // System UI (navigation/status bar) dimensions still remain zero at this point.
        // For the restoring job that needs these values, we wait till they get reported
        // correctly by posting the task instead of executing them right away.
        new Handler().post(this::restoreWindow);
    }

    @Override
    protected void drawDividerLine() {
        int width =
                mActivity.getResources().getDimensionPixelSize(R.dimen.custom_tabs_outline_width);
        boolean maxWidthBottomSheet = isMaxWidthLandscapeBottomSheet();
        int dividerSideInset = maxWidthBottomSheet ? width : 0;
        int dividerTopInset = shouldHaveNoShadowOffset() ? 0 : width;

        drawDividerLineBase(dividerSideInset, dividerTopInset, dividerSideInset);
    }

    @Override
    protected boolean shouldDrawDividerLine() {
        // Elevation shadows are only rendered properly on devices >= Android Q
        return SysUtils.isLowEndDevice() || Build.VERSION.SDK_INT < Build.VERSION_CODES.Q;
    }

    // Restore the window upon exiting fullscreen.
    private void restoreWindow() {
        initializeHeight();
        positionAtWidth(mVersionCompat.getDisplayWidth());
        updateShadowOffset();
        maybeInvokeResizeCallback();

        // Status/navigation bar are not restored on T+. This makes host app visible
        // at the area. To work around this, simulate user dragging the tab by 1 pixel
        // upon exiting fullscreen.
        if (!isFullHeight() && Build.VERSION.SDK_INT > Build.VERSION_CODES.S) {
            int startY = mActivity.getWindow().getAttributes().y;
            onDragStart(startY);
            onDragMove(startY + 1);
            onDragEnd(0);
        }
    }

    private boolean isLandscapeMaxWidth(int width) {
        float density = mActivity.getResources().getDisplayMetrics().density;
        return isLandscape() && width > BOTTOM_SHEET_MAX_WIDTH_DP_LANDSCAPE * density;
    }

    @Override
    public void destroy() {
        if (mContentScrollMayResizeTab && mTouchEventProvider.get() != null) {
            mTouchEventProvider.get().removeTouchEventObserver(this);
        }
    }

    void setMockViewForTesting(
            LinearLayout navbar,
            ImageView spinnerView,
            CircularProgressDrawable spinner,
            CustomTabToolbar toolbar,
            View toolbarCoordinator,
            PartialCustomTabHandleStrategyFactory handleStrategyFactory) {
        mNavbar = navbar;
        mSpinnerView = spinnerView;
        mSpinner = spinner;
        mToolbarView = toolbar;
        mToolbarCoordinator = toolbarCoordinator;
        mHandleStrategyFactory = handleStrategyFactory;

        mPositionUpdater = this::updatePosition;
        onPostInflationStartup();
    }

    int getNavbarHeightForTesting() {
        return mNavbarHeight;
    }

    CustomTabToolbar.HandleStrategy getHandleStrategyForTesting() {
        return mHandleStrategy;
    }

    CustomTabToolbar.HandleStrategy createHandleStrategyForTesting() {
        // Pass null for context because we don't depend on the GestureDetector inside as we invoke
        // MotionEvents directly in the tests.
        mHandleStrategy =
                new PartialCustomTabHandleStrategy(null, this::isFullHeight, () -> mStatus, this);
        return mHandleStrategy;
    }

    void setToolbarColorForTesting(int toolbarColor) {
        mToolbarColor = toolbarColor;
    }

    void setGestureObjectsForTesting(GestureDetector detector, ContentGestureListener listener) {
        mGestureDetector = detector;
        mGestureHandler = listener;
    }

    // Wrapper around Animator class, also holding the information to use after the animation ends.
    private static class TabAnimator {
        private final ValueAnimator mAnimator;
        private @HeightStatus int mTargetStatus;
        private boolean mAutoResize;

        private TabAnimator(
                ValueAnimator.AnimatorUpdateListener listener,
                int animTime,
                Runnable finishRunnable) {
            mAnimator = new ValueAnimator();
            mAnimator.addListener(
                    new AnimatorListenerAdapter() {
                        @Override
                        public void onAnimationStart(Animator animation) {}

                        @Override
                        public void onAnimationEnd(Animator animation) {
                            finishRunnable.run();
                        }
                    });
            mAnimator.addUpdateListener(listener);
            mAnimator.setInterpolator(new AccelerateInterpolator());
            mAnimator.setDuration(animTime);
        }

        private void start(int startY, int endY, int targetStatus, boolean autoResize) {
            mTargetStatus = targetStatus;
            mAutoResize = autoResize;
            mAnimator.setIntValues(startY, endY);
            mAnimator.start();
        }

        private @HeightStatus int getTargetStatus() {
            return mTargetStatus;
        }

        private boolean wasAutoResized() {
            return mAutoResize;
        }

        private void cancel() {
            mAnimator.cancel();
        }
    }
}
