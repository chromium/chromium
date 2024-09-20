// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.components.browser_ui.banners.SwipableOverlayView;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarAnimationListener;
import org.chromium.components.infobars.InfoBarContainerLayout;
import org.chromium.components.infobars.InfoBarUiItem;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;

/** The {@link View} for the {@link InfoBarContainer}. */
public class InfoBarContainerView extends SwipableOverlayView
        implements BrowserControlsStateProvider.Observer, InsetObserver.WindowInsetObserver {
    /** Observes container view changes. */
    public interface ContainerViewObserver extends InfoBarAnimationListener {
        /**
         * Called when the height of shown content changed.
         * @param shownFraction The ratio of height of shown content to the height of the container
         *                      view.
         */
        void onShownRatioChanged(float shownFraction);
    }

    /** Top margin, including the toolbar and tabstrip height and 48dp of web contents. */
    private static final int TOP_MARGIN_PHONE_DP = 104;

    private static final int TOP_MARGIN_TABLET_DP = 144;

    /** Length of the animation to fade the InfoBarContainer back into View. */
    private static final long REATTACH_FADE_IN_MS = 250;

    /** Whether or not the InfoBarContainer is allowed to hide when the user scrolls. */
    private static boolean sIsAllowedToAutoHide = true;

    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final ContainerViewObserver mContainerViewObserver;
    private final InfoBarContainerLayout mLayout;

    /** Parent view that contains the InfoBarContainerLayout. */
    private ViewGroup mParentView;

    /** Animation used to snap the container to the nearest state if scroll direction changes. */
    private Animator mScrollDirectionChangeAnimation;

    /** Whether or not the current scroll is downward. */
    private boolean mIsScrollingDownward;

    /** Tracks the previous event's scroll offset to determine if a scroll is up or down. */
    private int mLastScrollOffsetY;

    @Nullable private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier;
    @Nullable private EdgeToEdgePadAdjuster mEdgeToEdgePadAdjuster;

    /**
     * @param context The {@link Context} that this view is attached to.
     * @param containerViewObserver The {@link ContainerViewObserver} that gets notified on
     *     container view changes.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} that provides
     *     browser control offsets.
     * @param isTablet Whether this view is displayed on tablet or not.
     * @deprecated Please use the constructor with the EdgeToEdgeSupplier.
     */
    @Deprecated
    InfoBarContainerView(
            @NonNull Context context,
            @NonNull ContainerViewObserver containerViewObserver,
            @Nullable BrowserControlsStateProvider browserControlsStateProvider,
            boolean isTablet) {
        this(context, containerViewObserver, browserControlsStateProvider, null, isTablet);
    }

    /**
     * @param context The {@link Context} that this view is attached to.
     * @param containerViewObserver The {@link ContainerViewObserver} that gets notified on
     *     container view changes.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} that provides
     *     browser control offsets.
     * @param edgeToEdgeSupplier The supplier publishes the changes of the edge-to-edge state and
     *     the expected bottom paddings when edge-to-edge is on.
     * @param isTablet Whether this view is displayed on tablet or not.
     */
    InfoBarContainerView(
            @NonNull Context context,
            @NonNull ContainerViewObserver containerViewObserver,
            @Nullable BrowserControlsStateProvider browserControlsStateProvider,
            @Nullable ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            boolean isTablet) {
        super(context, null, false);
        mContainerViewObserver = containerViewObserver;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        if (mBrowserControlsStateProvider != null) {
            mBrowserControlsStateProvider.addObserver(this);
        }
        mEdgeToEdgeSupplier = edgeToEdgeSupplier;

        // TODO(newt): move this workaround into the infobar views if/when they're scrollable.
        // Workaround for http://crbug.com/407149. See explanation in onMeasure() below.
        setVerticalScrollBarEnabled(false);

        updateLayoutParams(context, isTablet);

        Runnable makeContainerVisibleRunnable = () -> runUpEventAnimation(true);
        mLayout =
                new InfoBarContainerLayout(
                        context,
                        makeContainerVisibleRunnable,
                        new InfoBarAnimationListener() {
                            @Override
                            public void notifyAnimationFinished(int animationType) {
                                mContainerViewObserver.notifyAnimationFinished(animationType);
                            }

                            @Override
                            public void notifyAllAnimationsFinished(InfoBarUiItem frontInfoBar) {
                                mContainerViewObserver.notifyAllAnimationsFinished(frontInfoBar);
                            }
                        });

        addView(
                mLayout,
                new FrameLayout.LayoutParams(
                        LayoutParams.MATCH_PARENT,
                        LayoutParams.WRAP_CONTENT,
                        Gravity.CENTER_HORIZONTAL));
    }

    void destroy() {
        if (mBrowserControlsStateProvider != null) {
            mBrowserControlsStateProvider.removeObserver(this);
        }
        removeFromParentView();
    }

    // SwipableOverlayView implementation.
    @Override
    protected boolean isAllowedToAutoHide() {
        return sIsAllowedToAutoHide;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (getVisibility() != View.GONE) {
            setVisibility(VISIBLE);
            setAlpha(0f);
            animate().alpha(1f).setDuration(REATTACH_FADE_IN_MS);
        }
    }

    @Override
    protected boolean shouldConsumeScroll(int scrollOffsetY, int scrollExtentY) {
        if (mBrowserControlsStateProvider.getBottomControlsHeight() <= 0) return true;

        boolean isScrollingDownward = scrollOffsetY > mLastScrollOffsetY;
        boolean didDirectionChange = isScrollingDownward != mIsScrollingDownward;
        mLastScrollOffsetY = scrollOffsetY;
        mIsScrollingDownward = isScrollingDownward;

        // If the scroll changed directions, snap to a completely shown or hidden state.
        if (didDirectionChange) {
            runDirectionChangeAnimation(shouldSnapToVisibleState(scrollOffsetY));
            return false;
        }

        boolean areControlsCompletelyShown =
                mBrowserControlsStateProvider.getBottomControlOffset() > 0;
        boolean areControlsCompletelyHidden =
                BrowserControlsUtils.areBrowserControlsOffScreen(mBrowserControlsStateProvider);

        if ((!mIsScrollingDownward && areControlsCompletelyShown)
                || (mIsScrollingDownward && !areControlsCompletelyHidden)) {
            return false;
        }

        return true;
    }

    @Override
    protected void runUpEventAnimation(boolean visible) {
        if (mScrollDirectionChangeAnimation != null) mScrollDirectionChangeAnimation.cancel();
        super.runUpEventAnimation(visible);
    }

    @Override
    protected boolean isIndependentlyAnimating() {
        return mScrollDirectionChangeAnimation != null;
    }

    // View implementation.
    @Override
    public void setTranslationY(float translationY) {
        super.setTranslationY(translationY);
        float shownFraction = getHeight() > 0 ? 1f - (translationY / getHeight()) : 0;
        mContainerViewObserver.onShownRatioChanged(shownFraction);
    }

    // BrowserControlsStateProvider.Observer implementation.
    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean needsAnimate,
            boolean isVisibilityForced) {
        if (!isAllowedToAutoHide()) {
            return;
        }
        setTranslationY(
                getTotalHeight()
                        * 1.0f
                        * Math.abs(topOffset)
                        / mBrowserControlsStateProvider.getTopControlsHeight());
    }

    /**
     * Sets whether the InfoBarContainer is allowed to auto-hide when the user scrolls the page.
     * Expected to be called when Touch Exploration is enabled.
     * @param isAllowed Whether auto-hiding is allowed.
     */
    public static void setIsAllowedToAutoHide(boolean isAllowed) {
        sIsAllowedToAutoHide = isAllowed;
    }

    /**
     * Notifies that an infobar's View ({@link InfoBar#getView}) has changed. If the infobar is
     * visible, a view swapping animation will be run.
     */
    void notifyInfoBarViewChanged() {
        mLayout.notifyInfoBarViewChanged();
    }

    /** Sets the parent {@link ViewGroup} that contains the {@link InfoBarContainer}. */
    void setParentView(ViewGroup parent) {
        mParentView = parent;
        // Don't attach the container to the new parent if it is not previously attached.
        if (removeFromParentView()) addToParentView();
    }

    /** Adds this class to the parent view {@link #mParentView}. */
    void addToParentView() {
        super.addToParentView(mParentView);
    }

    /**
     * Adds an {@link InfoBar} to the layout.
     * @param infoBar The {@link InfoBar} to be added.
     */
    void addInfoBar(InfoBar infoBar) {
        View infoBarView = infoBar.createView();
        mLayout.addInfoBar(infoBar);

        if (mEdgeToEdgeSupplier != null) {
            mEdgeToEdgePadAdjuster =
                    EdgeToEdgeControllerFactory.createForViewAndObserveSupplier(
                            infoBarView, mEdgeToEdgeSupplier);
        }
    }

    /**
     * Removes an {@link InfoBar} from the layout.
     *
     * @param infoBar The {@link InfoBar} to be removed.
     */
    void removeInfoBar(InfoBar infoBar) {
        mLayout.removeInfoBar(infoBar);
        if (mEdgeToEdgeSupplier != null) {
            assert (mEdgeToEdgePadAdjuster != null);
            mEdgeToEdgePadAdjuster.destroy();
        }
    }

    /**
     * Hides or stops hiding this View.
     * @param isHidden Whether this View is should be hidden.
     */
    void setHidden(boolean isHidden) {
        setVisibility(isHidden ? View.GONE : View.VISIBLE);
    }

    /**
     * Run an animation when the scrolling direction of a gesture has changed (this does not mean
     * the gesture has ended).
     * @param visible Whether or not the view should be visible.
     */
    private void runDirectionChangeAnimation(boolean visible) {
        mScrollDirectionChangeAnimation = createVerticalSnapAnimation(visible);
        mScrollDirectionChangeAnimation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mScrollDirectionChangeAnimation = null;
                    }
                });
        mScrollDirectionChangeAnimation.start();
    }

    private void updateLayoutParams(Context context, boolean isTablet) {
        LayoutParams lp =
                new LayoutParams(
                        LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT, Gravity.BOTTOM);
        int topMarginDp = isTablet ? TOP_MARGIN_TABLET_DP : TOP_MARGIN_PHONE_DP;
        lp.topMargin = DisplayUtil.dpToPx(DisplayAndroid.getNonMultiDisplay(context), topMarginDp);
        setLayoutParams(lp);
    }

    /** Returns true if any animations are pending or in progress. */
    @VisibleForTesting
    public boolean isAnimating() {
        return mLayout.isAnimating();
    }
}
