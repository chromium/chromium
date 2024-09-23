// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.graphics.Rect;
import android.util.SparseIntArray;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.DestroyableObservableSupplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsMarginSupplier;

import java.util.Comparator;
import java.util.PriorityQueue;

/**
 * This class is responsible for displaying custom {@link View}s on top of {@link Tab}'s Content
 * view. Users that want to display a custom {@link View} should:
 *     1. Implement {@link TabViewProvider}
 *     2. Add an entry to {@link TabViewProvider.Type}
 *     3. Add their {@link TabViewProvider.Type} to {@link #PRIORITIZED_TAB_VIEW_PROVIDER_TYPES}
 *        with an appropriate priority. In order to find the right priority, please consider the
 *        existing entries in the array and determine where the new feature fits relative to them.
 *     4. Use {@link Tab#getTabViewManager#addTabViewProvider} and
 *        {@link Tab#getTabViewManager#removeTabViewProvider} to add and remove their
 *        {@link TabViewProvider}.
 */
class TabViewManagerImpl implements TabViewManager, Comparator<TabViewProvider> {
    /**
     * A prioritized list of all {@link TabViewProvider.Type}s, from most important to least
     * important. The {@link TabViewProvider} with the highest priority will always be shown first,
     * regardless of its insertion time relative to other {@link TabViewProvider}s.
     */
    @VisibleForTesting @TabViewProvider.Type
    static final int[] PRIORITIZED_TAB_VIEW_PROVIDER_TYPES =
            new int[] {
                TabViewProvider.Type.SUSPENDED_TAB,
                TabViewProvider.Type.PAINT_PREVIEW,
                TabViewProvider.Type.SAD_TAB
            };

    /**
     * A lookup table for {@link #PRIORITIZED_TAB_VIEW_PROVIDER_TYPES}. This is initialized in the
     * following static block and doesn't need to be manually updated.
     */
    private static final SparseIntArray TAB_VIEW_PROVIDER_PRIORITY_LOOKUP = new SparseIntArray();

    static {
        for (int i = 0; i < PRIORITIZED_TAB_VIEW_PROVIDER_TYPES.length; i++) {
            TAB_VIEW_PROVIDER_PRIORITY_LOOKUP.put(PRIORITIZED_TAB_VIEW_PROVIDER_TYPES[i], i);
        }
    }

    private PriorityQueue<TabViewProvider> mTabViewProviders;
    private TabImpl mTab;
    private View mCurrentView;
    private DestroyableObservableSupplier<Rect> mMarginSupplier;
    private final Rect mViewMargins = new Rect();

    TabViewManagerImpl(TabImpl tab) {
        mTab = tab;
        mTabViewProviders = new PriorityQueue<>(PRIORITIZED_TAB_VIEW_PROVIDER_TYPES.length, this);
    }

    private void initMarginSupplier() {
        if (mTab.getActivity() == null
                || mTab.getActivity().isActivityFinishingOrDestroyed()
                || mMarginSupplier != null) {
            return;
        }

        mMarginSupplier =
                new BrowserControlsMarginSupplier(mTab.getActivity().getBrowserControlsManager());
        mMarginSupplier.addObserver(this::updateViewMargins);
        // Update margins immediately if available rather than waiting for a posted notification.
        // Waiting for a posted notification could allow a layout pass to occur before the margins
        // are set.
        updateViewMargins(mMarginSupplier.get());
    }

    /**
     * @return Whether the given {@link TabViewProvider} is currently being displayed.
     */
    @Override
    public boolean isShowing(TabViewProvider tabViewProvider) {
        TabViewProvider currentTVP = mTabViewProviders.peek();
        return currentTVP != null && currentTVP == tabViewProvider;
    }

    /**
     * Adds a {@link TabViewProvider} to be shown in the {@link Tab} associated with this {@link
     * TabViewManager}. If the given {@link TabViewProvider} has the highest priority, it will be
     * shown immediately. Otherwise, it will be shown after other {@link TabViewProvider}s with
     * higher priorities are removed.
     */
    @Override
    public void addTabViewProvider(TabViewProvider tabViewProvider) {
        if (mTabViewProviders.contains(tabViewProvider)) return;

        TabViewProvider currentTabViewProvider = mTabViewProviders.peek();
        mTabViewProviders.add(tabViewProvider);
        updateCurrentTabViewProvider(currentTabViewProvider);
    }

    /**
     * Remove the given {@link TabViewProvider} from the {@link Tab} associated with this {@link
     * TabViewManager}. If the given {@link TabViewProvider} is currently shown, the next available
     * {@link TabViewProvider} with the highest priority will be shown. If there are no other {@link
     * TabViewProvider}s, {@link Tab}'s Content view will be shown.
     */
    @Override
    public void removeTabViewProvider(TabViewProvider tabViewProvider) {
        TabViewProvider currentTabViewProvider = mTabViewProviders.peek();
        mTabViewProviders.remove(tabViewProvider);
        updateCurrentTabViewProvider(currentTabViewProvider);
    }

    private void updateCurrentTabViewProvider(TabViewProvider previousTabViewProvider) {
        if (mTab == null) return;

        TabViewProvider currentTabViewProvider = mTabViewProviders.peek();
        if (currentTabViewProvider != previousTabViewProvider) {
            View view = null;
            @ColorInt Integer backgroundColor = null;
            if (currentTabViewProvider != null) {
                view = currentTabViewProvider.getView();
                assert view != null;
                view.setFocusable(true);
                view.setFocusableInTouchMode(true);
                backgroundColor = currentTabViewProvider.getBackgroundColor(view.getContext());
            }
            mCurrentView = view;
            initMarginSupplier();
            updateViewMargins();
            mTab.setCustomView(mCurrentView, backgroundColor);
            if (previousTabViewProvider != null) previousTabViewProvider.onHidden();
            if (currentTabViewProvider != null) currentTabViewProvider.onShown();
        }
    }

    private void updateViewMargins(Rect viewMargins) {
        if (viewMargins == null) return;

        mViewMargins.set(viewMargins);
        updateViewMargins();
    }

    private void updateViewMargins() {
        if (mCurrentView == null) return;

        FrameLayout.LayoutParams layoutParams =
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT);
        layoutParams.setMargins(
                mViewMargins.left, mViewMargins.top, mViewMargins.right, mViewMargins.bottom);
        mCurrentView.setLayoutParams(layoutParams);
    }

    /**
     * Compares two {@link TabViewProvider}s based on their priority in
     * {@link #PRIORITIZED_TAB_VIEW_PROVIDER_TYPES}. Do not edit the logic here when you add a new
     * {@link TabViewProvider.Type}. Instead, simply add your new {@link TabViewProvider.Type} to
     * {@link #PRIORITIZED_TAB_VIEW_PROVIDER_TYPES}.
     */
    @Override
    public int compare(TabViewProvider tvp1, TabViewProvider tvp2) {
        int tvp1Priority = TAB_VIEW_PROVIDER_PRIORITY_LOOKUP.get(tvp1.getTabViewProviderType());
        int tvp2Priority = TAB_VIEW_PROVIDER_PRIORITY_LOOKUP.get(tvp2.getTabViewProviderType());
        return tvp1Priority - tvp2Priority;
    }

    void destroy() {
        mTab.setCustomView(null, null);
        TabViewProvider currentTabViewProvider = mTabViewProviders.peek();
        if (currentTabViewProvider != null) currentTabViewProvider.onHidden();
        mTabViewProviders.clear();
        if (mMarginSupplier != null) mMarginSupplier.destroy();
        mTab = null;
    }
}
