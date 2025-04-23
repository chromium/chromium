// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabContextMenuData;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.url.GURL;

/**
 * Interruptor logic for the {@link NewTabAnimationLayout} new background tab animation. Interrupts
 * the animation and forces it to finish for the cases of
 *
 * <ul>
 *   <li>Tab being animated over is navigated.
 *   <li>Tab is switched.
 *   <li>A different layout is shown.
 * </ul>
 */
@NullMarked
class AnimationInterruptor implements Destroyable {
    private final LayoutStateObserver mLayoutStateObserver =
            new LayoutStateObserver() {
                @Override
                public void onStartedShowing(@LayoutType int layoutType) {
                    if (layoutType == LayoutType.TAB_SWITCHER
                            || layoutType == LayoutType.TOOLBAR_SWIPE) {
                        interruptAnimation();
                    }
                    // Ignore BROWSING as that is what we are showing over and a new
                    // SIMPLE_ANIMATION is
                    // already handled elsewhere.
                }
            };

    private final Callback<Tab> mCurrentTabObserver = this::onCurrentTabChanged;

    private final TabObserver mTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onPageLoadStarted(Tab tab, GURL url) {
                    interruptAnimation();
                }
            };

    private final Callback<Boolean> mScrimVisibilityObserver = this::onScrimVisibilityChanged;

    private final LayoutStateProvider mLayoutStateProvider;
    private final ObservableSupplier<Tab> mCurrentTabSupplier;
    private final Tab mAnimationTab;
    private final ObservableSupplier<Boolean> mScrimVisibilitySupplier;
    private final ObservableSupplier<Boolean> mContextMenuVisibilitySupplier;

    private @Nullable Runnable mInterruptAnimationRunnable;

    /**
     * @param layoutStateProvider To determine when layout states change.
     * @param currentTabSupplier To determine when a tab switch happens.
     * @param animationTab To observe for navigations.
     * @param interruptAnimationRunnable Invoked when the animation should be interrupted.
     */
    AnimationInterruptor(
            LayoutStateProvider layoutStateProvider,
            ObservableSupplier<Tab> currentTabSupplier,
            Tab animationTab,
            ObservableSupplier<Boolean> scrimVisibilitySupplier,
            Runnable interruptAnimationRunnable) {
        mLayoutStateProvider = layoutStateProvider;
        mCurrentTabSupplier = currentTabSupplier;
        mAnimationTab = animationTab;
        mScrimVisibilitySupplier = scrimVisibilitySupplier;
        TabContextMenuData data = TabContextMenuData.getForTab(animationTab);
        if (data == null) {
            mContextMenuVisibilitySupplier = new ObservableSupplierImpl<>(false);
        } else {
            mContextMenuVisibilitySupplier = data.getTabContextMenuVisibilitySupplier();
        }
        mInterruptAnimationRunnable = interruptAnimationRunnable;

        mLayoutStateProvider.addObserver(mLayoutStateObserver);
        mCurrentTabSupplier.addSyncObserver(mCurrentTabObserver);
        mAnimationTab.addObserver(mTabObserver);
        mScrimVisibilitySupplier.addSyncObserver(mScrimVisibilityObserver);
        mContextMenuVisibilitySupplier.addSyncObserver(mScrimVisibilityObserver);
    }

    @Override
    public void destroy() {
        mInterruptAnimationRunnable = null;
        mLayoutStateProvider.removeObserver(mLayoutStateObserver);
        mCurrentTabSupplier.removeObserver(mCurrentTabObserver);
        mAnimationTab.removeObserver(mTabObserver);
        mScrimVisibilitySupplier.removeObserver(mScrimVisibilityObserver);
        mContextMenuVisibilitySupplier.removeObserver(mScrimVisibilityObserver);
    }

    private void onCurrentTabChanged(Tab tab) {
        if (mAnimationTab == tab) return;
        interruptAnimation();
    }

    private void onScrimVisibilityChanged(Boolean visible) {
        if (!Boolean.TRUE.equals(visible)) return;
        interruptAnimation();
    }

    private void interruptAnimation() {
        if (mInterruptAnimationRunnable == null) return;

        // Protect against re-entrancy if multiple observers are called concurrently.
        Runnable r = mInterruptAnimationRunnable;
        destroy();
        r.run();
    }
}
