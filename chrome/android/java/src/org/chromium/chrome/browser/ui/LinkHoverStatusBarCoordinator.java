// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewPropertyAnimator;
import android.view.ViewStub;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

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
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

/** Coordinator for the link hover status bar. */
@NullMarked
public class LinkHoverStatusBarCoordinator extends EmptyTabObserver {
    private static final int EXPAND_HOVER_DELAY_MS = 1600;
    private static final int FADE_IN_DURATION_MS = 120;
    private static final int FADE_OUT_DURATION_MS = 200;
    private static final int HIDE_DELAY_MS = 250;

    private final CurrentTabObserver mCurrentTabObserver;
    private final TextView mLinkHoverStatusBar;
    private final Context mContext;
    private final Drawable mBackgroundDrawable;
    private final int mInitialMaxWidth;
    private final int mHorizontalMargin;

    private GURL mCurrentUrl = GURL.emptyGURL();
    private @Nullable CancelableRunnable mExpandStatusBarCancelableRunnable;
    private @Nullable ViewPropertyAnimator mStatusBarAnimator;
    private @Nullable CancelableRunnable mHideRunnable;

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

        mLinkHoverStatusBar = (TextView) statusBarStub.inflate();
        mBackgroundDrawable =
                AppCompatResources.getDrawable(mContext, R.drawable.link_status_bar_background)
                        .mutate();
        mInitialMaxWidth =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.link_hover_status_bar_max_width);
        mHorizontalMargin =
                mContext.getResources().getDimensionPixelSize(R.dimen.link_hover_status_bar_margin);
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

    /** Destroy the coordinator. */
    public void destroy() {
        if (mExpandStatusBarCancelableRunnable != null) {
            mExpandStatusBarCancelableRunnable.cancel();
            mExpandStatusBarCancelableRunnable = null;
        }
        mCurrentTabObserver.destroy();
        if (mStatusBarAnimator != null) {
            mStatusBarAnimator.cancel();
            mStatusBarAnimator = null;
        }
        if (mHideRunnable != null) {
            mHideRunnable.cancel();
            mHideRunnable = null;
        }
    }

    private void expandStatusBar() {
        int windowWidth = mContext.getResources().getDisplayMetrics().widthPixels;
        mLinkHoverStatusBar.setMaxWidth(windowWidth - 2 * mHorizontalMargin);
    }
}
