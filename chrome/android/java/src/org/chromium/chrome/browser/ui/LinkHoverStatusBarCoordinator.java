// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewPropertyAnimator;
import android.view.ViewStub;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.coordinatorlayout.widget.CoordinatorLayout;

import org.chromium.base.Callback;
import org.chromium.base.CancelableRunnable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

/** Coordinator for the link hover status bar. */
@NullMarked
public class LinkHoverStatusBarCoordinator extends EmptyTabObserver
        implements View.OnHoverListener {
    private static final int EXPAND_HOVER_DELAY_MS = 1600;
    private static final int FADE_IN_DURATION_MS = 120;
    private static final int FADE_OUT_DURATION_MS = 200;
    private static final int HIDE_DELAY_MS = 250;

    private final CurrentTabObserver mCurrentTabObserver;
    private final TextView mLinkHoverStatusBar;
    private final Context mContext;
    private final Drawable mBackgroundDrawable;
    private final int mInitialMaxWidth;
    private final int mMargin;
    private final int mMousePadding;
    private final ObservableSupplier<@Nullable Tab> mTabProvider;
    private final Callback<@Nullable Tab> mTabSupplierObserver;

    private GURL mCurrentUrl = GURL.emptyGURL();
    private @Nullable CancelableRunnable mExpandStatusBarCancelableRunnable;
    private @Nullable ViewPropertyAnimator mStatusBarAnimator;
    private @Nullable CancelableRunnable mHideRunnable;
    private float mCursorRawX;
    private float mCursorRawY;
    private @Nullable ContentView mTrackedView;

    private final Runnable mAdjustStatusBarPositionRunnable =
            this::adjustStatusBarPositionForCursor;

    /**
     * @param context The current context.
     * @param tabProvider The provider of the current tab.
     * @param statusBarStub The {@link ViewStub} for the status bar.
     */
    public LinkHoverStatusBarCoordinator(
            Context context,
            ObservableSupplier<@Nullable Tab> tabProvider,
            ViewStub statusBarStub) {
        mContext = context;
        mCurrentTabObserver = new CurrentTabObserver(tabProvider, this);
        mTabProvider = tabProvider;
        mTabSupplierObserver = (tab) -> updateHoverListener();
        mTabProvider.addObserver(mTabSupplierObserver);

        mLinkHoverStatusBar = (TextView) statusBarStub.inflate();
        mBackgroundDrawable =
                AppCompatResources.getDrawable(mContext, R.drawable.link_status_bar_background)
                        .mutate();
        mInitialMaxWidth =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.link_hover_status_bar_max_width);
        mMargin =
                mContext.getResources().getDimensionPixelSize(R.dimen.link_hover_status_bar_margin);
        mMousePadding =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.link_hover_status_bar_mouse_padding);

        updateHoverListener();
    }

    @Override
    public void onContentChanged(Tab tab) {
        updateHoverListener();
    }

    private void updateHoverListener() {
        Tab tab = mTabProvider.get();
        ContentView newView = tab != null ? tab.getContentView() : null;
        if (mTrackedView == newView) return;

        if (mTrackedView != null) {
            mTrackedView.removeOnHoverListener(this);
        }
        mTrackedView = newView;
        if (mTrackedView != null) {
            mTrackedView.addOnHoverListener(this);
        }
    }

    @Override
    public void onUpdateTargetUrl(Tab tab, GURL url) {
        // This method may be called with the same URL when the page is refreshed.
        if (url.equals(mCurrentUrl)) return;
        mCurrentUrl = url;

        // Cancel any pending animations or runnables from the previous URL update.
        if (mStatusBarAnimator != null) {
            mStatusBarAnimator.cancel();
            mStatusBarAnimator = null;
        }
        if (mHideRunnable != null) {
            mHideRunnable.cancel();
            mHideRunnable = null;
        }
        if (mExpandStatusBarCancelableRunnable != null) {
            mExpandStatusBarCancelableRunnable.cancel();
            mExpandStatusBarCancelableRunnable = null;
        }

        if (!url.isEmpty()) {
            mLinkHoverStatusBar.setText(mCurrentUrl.getSpec());
            boolean isIncognito = tab.isIncognitoBranded();
            boolean isNightMode = ColorUtils.inNightMode(mContext);
            if (isIncognito || isNightMode) {
                mBackgroundDrawable.setTint(SemanticColorUtils.getDefaultBgColor(mContext));
                mLinkHoverStatusBar.setTextAppearance(R.style.TextAppearance_TextMedium_Primary);
            } else {
                mBackgroundDrawable.setTint(
                        SemanticColorUtils.getDefaultControlColorActive(mContext));
                mLinkHoverStatusBar.setTextAppearance(
                        R.style.TextAppearance_TextMedium_Primary_OnAccent1);
            }
            mLinkHoverStatusBar.setBackground(mBackgroundDrawable);
            mLinkHoverStatusBar.setMaxWidth(mInitialMaxWidth);

            // TODO(crbug.com/454446656): Move the status bar to avoid the cursor.
            if (mLinkHoverStatusBar.getVisibility() != View.VISIBLE) {
                mLinkHoverStatusBar.setAlpha(0f);
                mLinkHoverStatusBar.setVisibility(View.VISIBLE);
                mStatusBarAnimator =
                        mLinkHoverStatusBar
                                .animate()
                                .alpha(1f)
                                .setDuration(FADE_IN_DURATION_MS)
                                .setListener(
                                        new AnimatorListenerAdapter() {
                                            @Override
                                            public void onAnimationEnd(Animator animation) {
                                                mStatusBarAnimator = null;
                                            }
                                        });
                mStatusBarAnimator.start();
            } else {
                // If the status bar is already visible, just make sure it's fully opaque.
                // This is for the case where a fade-out animation was cancelled.
                mLinkHoverStatusBar.setAlpha(1f);
            }

            mExpandStatusBarCancelableRunnable = new CancelableRunnable(this::expandStatusBar);
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT,
                    mExpandStatusBarCancelableRunnable,
                    EXPAND_HOVER_DELAY_MS);

            // Ensure the position is adjusted after the layout is updated with the new text.
            mLinkHoverStatusBar.addOnLayoutChangeListener(
                    new View.OnLayoutChangeListener() {
                        @Override
                        public void onLayoutChange(
                                View v,
                                int left,
                                int top,
                                int right,
                                int bottom,
                                int oldLeft,
                                int oldTop,
                                int oldRight,
                                int oldBottom) {
                            mLinkHoverStatusBar.removeOnLayoutChangeListener(this);
                            adjustStatusBarPositionForCursor();
                        }
                    });
        } else {
            Runnable hideStatusBar =
                    () -> {
                        mStatusBarAnimator =
                                mLinkHoverStatusBar
                                        .animate()
                                        .alpha(0f)
                                        .setDuration(FADE_OUT_DURATION_MS)
                                        .setListener(
                                                new AnimatorListenerAdapter() {
                                                    @Override
                                                    public void onAnimationEnd(Animator animation) {
                                                        mLinkHoverStatusBar.setVisibility(
                                                                View.GONE);
                                                        mStatusBarAnimator = null;
                                                    }
                                                });
                        mStatusBarAnimator.start();
                    };
            mHideRunnable =
                    new CancelableRunnable(
                            () -> {
                                hideStatusBar.run();
                                mHideRunnable = null;
                            });
            PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, mHideRunnable, HIDE_DELAY_MS);
        }
    }

    @Override
    public boolean onHover(View view, MotionEvent event) {
        onCursorPositionChanged(event.getRawX(), event.getRawY());
        // Return false so the event is not consumed and can be passed to
        // other listeners.
        return false;
    }

    /**
     * Called when the cursor position changes.
     *
     * @param rawX The raw X coordinate of the cursor on the screen.
     * @param rawY The raw Y coordinate of the cursor on the screen.
     */
    private void onCursorPositionChanged(float rawX, float rawY) {
        if (mCursorRawX == rawX && mCursorRawY == rawY) return;
        mCursorRawX = rawX;
        mCursorRawY = rawY;

        // Post the adjustment to the message queue to ensure it runs after any pending
        // layout passes (e.g. triggered by setText) are processed.
        mLinkHoverStatusBar.removeCallbacks(mAdjustStatusBarPositionRunnable);
        mLinkHoverStatusBar.post(mAdjustStatusBarPositionRunnable);
    }

    /** Destroy the coordinator. */
    public void destroy() {
        if (mExpandStatusBarCancelableRunnable != null) {
            mExpandStatusBarCancelableRunnable.cancel();
            mExpandStatusBarCancelableRunnable = null;
        }
        mCurrentTabObserver.destroy();
        mTabProvider.removeObserver(mTabSupplierObserver);
        if (mStatusBarAnimator != null) {
            mStatusBarAnimator.cancel();
            mStatusBarAnimator = null;
        }
        if (mHideRunnable != null) {
            mHideRunnable.cancel();
            mHideRunnable = null;
        }
        if (mTrackedView != null) {
            mTrackedView.removeOnHoverListener(this);
            mTrackedView = null;
        }
    }

    private void expandStatusBar() {
        int windowWidth = mContext.getResources().getDisplayMetrics().widthPixels;
        mLinkHoverStatusBar.setMaxWidth(windowWidth - 2 * mMargin);
    }

    private void adjustStatusBarPositionForCursor() {
        if (mLinkHoverStatusBar.getVisibility() != View.VISIBLE
                || mLinkHoverStatusBar.getWidth() == 0) {
            return;
        }

        // Determine the bounds of the status bar as if it were in the default position
        // (gravity = BOTTOM | START, translationY = 0).
        View parent = (View) mLinkHoverStatusBar.getParent();
        int[] parentLoc = new int[2];
        parent.getLocationOnScreen(parentLoc);

        int width = mLinkHoverStatusBar.getWidth();
        int height = mLinkHoverStatusBar.getHeight();
        int parentWidth = parent.getWidth();
        int parentHeight = parent.getHeight();

        boolean isRtl = mLinkHoverStatusBar.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL;

        int defaultLeft;
        if (isRtl) {
            defaultLeft = parentLoc[0] + parentWidth - mMargin - width;
        } else {
            defaultLeft = parentLoc[0] + mMargin;
        }

        int defaultBottom = parentLoc[1] + parentHeight - mMargin;
        int defaultTop = defaultBottom - height;
        int defaultRight = defaultLeft + width;

        Rect defaultRect = new Rect(defaultLeft, defaultTop, defaultRight, defaultBottom);
        Rect avoidanceRect = new Rect(defaultRect);
        avoidanceRect.inset(-mMousePadding, -mMousePadding);

        if (avoidanceRect.contains((int) mCursorRawX, (int) mCursorRawY)) {
            moveBarToBottomRight();
        } else {
            // Cursor is outside the avoidance zone, reset to default position.
            resetBarPosition();
        }
    }

    private void moveBarToBottomRight() {
        CoordinatorLayout.LayoutParams params =
                (CoordinatorLayout.LayoutParams) mLinkHoverStatusBar.getLayoutParams();
        if (params.gravity != (Gravity.BOTTOM | Gravity.END)) {
            params.gravity = Gravity.BOTTOM | Gravity.END;
            mLinkHoverStatusBar.setLayoutParams(params);
        }
    }

    private void resetBarPosition() {

        CoordinatorLayout.LayoutParams params =
                (CoordinatorLayout.LayoutParams) mLinkHoverStatusBar.getLayoutParams();
        if (params.gravity != (Gravity.BOTTOM | Gravity.START)) {
            params.gravity = Gravity.BOTTOM | Gravity.START;
            mLinkHoverStatusBar.setLayoutParams(params);
        }
    }
}
