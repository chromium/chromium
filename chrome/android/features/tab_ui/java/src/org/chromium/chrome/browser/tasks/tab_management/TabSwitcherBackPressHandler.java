// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.chrome.features.start_surface.StartSurfaceState;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

/**
 * A {@link BackPressHandler} intercepting back press to show {@link LayoutType#BROWSING} when
 * the tab switcher is showing.
 */
public class TabSwitcherBackPressHandler
        implements BackPressHandler, StartSurface.StateObserver, LayoutStateObserver, Destroyable {
    private final OneshotSupplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private final OneshotSupplier<StartSurface> mStartSurfaceSupplier;
    private final Runnable mShowBrowsing;
    private final boolean mIsStartSurfaceRefactorEnabled;
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    public TabSwitcherBackPressHandler(
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            OneshotSupplier<StartSurface> startSurfaceSupplier, Runnable showBrowsing,
            boolean isStartSurfaceRefactorEnabled) {
        layoutStateProviderSupplier.onAvailable(this::onLayoutStateProviderAvailable);
        startSurfaceSupplier.onAvailable(this::onStartSurfaceAvailable);
        mLayoutStateProviderSupplier = layoutStateProviderSupplier;
        mStartSurfaceSupplier = startSurfaceSupplier;
        mShowBrowsing = showBrowsing;
        mIsStartSurfaceRefactorEnabled = isStartSurfaceRefactorEnabled;
        onBackPressChanged();
    }

    @Override
    public void onStateChanged(int startSurfaceState, boolean shouldShowTabSwitcherToolbar) {
        onBackPressChanged();
    }

    @Override
    public void onStartedShowing(int layoutType, boolean showToolbar) {
        onBackPressChanged();
    }

    @Override
    public void handleBackPress() {
        mShowBrowsing.run();
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressChangedSupplier;
    }

    @Override
    public void destroy() {
        if (mLayoutStateProviderSupplier.hasValue()) {
            mLayoutStateProviderSupplier.get().removeObserver(this);
        }
        if (mStartSurfaceSupplier.hasValue()) {
            mStartSurfaceSupplier.get().removeStateChangeObserver(this);
        }
    }

    private void onLayoutStateProviderAvailable(LayoutStateProvider provider) {
        provider.addObserver(this);
        onBackPressChanged();
    }

    private void onStartSurfaceAvailable(StartSurface startSurface) {
        if (mIsStartSurfaceRefactorEnabled) return;
        // TODO(crbug.com/1315679): Remove |mStartSurfaceSupplier| and
        // StartSurfaceStateChangeObserver when refactor flag is enabled by default.
        startSurface.addStateChangeObserver(this);
        onBackPressChanged();
    }

    private void onBackPressChanged() {
        LayoutStateProvider provider = mLayoutStateProviderSupplier.get();
        boolean isOverviewVisible = provider != null
                && (provider.isLayoutVisible(LayoutType.TAB_SWITCHER)
                        || provider.isLayoutVisible(LayoutType.START_SURFACE));
        if (!isOverviewVisible) {
            mBackPressChangedSupplier.set(false);
            return;
        }
        StartSurface startSurface = mStartSurfaceSupplier.get();
        boolean isStartSurfaceShownTabSwitcher = mIsStartSurfaceRefactorEnabled
                ? provider.isLayoutVisible(LayoutType.TAB_SWITCHER)
                : startSurface == null
                        || startSurface.getStartSurfaceState()
                                == StartSurfaceState.SHOWN_TABSWITCHER;
        mBackPressChangedSupplier.set(isStartSurfaceShownTabSwitcher);
    }
}
