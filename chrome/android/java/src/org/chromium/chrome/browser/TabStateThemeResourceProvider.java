// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplier.NotifyBehavior;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.theme.ThemeResourceWrapper;

/**
 * A theme resource provider that provides #getTheme, #getResources, and layout inflator based on
 * the current tab model. When current layout is {@link LayoutType#BROWSING}, the theme resource
 * overlay will enable if the tab is incognito branded; otherwise, the theme resource overlay will
 * be disabled.
 */
@NullMarked
public class TabStateThemeResourceProvider extends ThemeResourceWrapper {
    private final ActivityTabProvider mActivityTabProvider;
    private final ObservableSupplier<LayoutManagerImpl> mLayoutManagerSupplier;
    private final LayoutStateObserver mLayoutStateObserver;
    private final Callback<@Nullable Tab> mTabCallback = this::maybeUpdateOverlay;
    private final Callback<LayoutManagerImpl> mLayoutManagerChangeCallback =
            new ValueChangedCallback<>(this::updateLayoutManager);

    private @Nullable LayoutManagerImpl mLayoutManager;
    private @Nullable Tab mLatestTab;

    /**
     * Create the instance based on the base context.
     *
     * @param baseContext The base context to be wrapped.
     * @param resourceId The theme overlay resource to be used for the overlay.
     */
    public TabStateThemeResourceProvider(
            Context baseContext,
            int resourceId,
            ActivityTabProvider activityTabProvider,
            ObservableSupplier<LayoutManagerImpl> layoutManagerSupplier) {
        super(baseContext, resourceId);

        mActivityTabProvider = activityTabProvider;
        mLayoutManagerSupplier = layoutManagerSupplier;

        mLayoutStateObserver =
                new LayoutStateObserver() {
                    @Override
                    public void onFinishedShowing(int layoutType) {
                        if (layoutType != LayoutType.BROWSING || mLatestTab == null) return;
                        maybeUpdateOverlay(mLatestTab);
                    }

                    @Override
                    public void onStartedHiding(int layoutType) {
                        if (layoutType != LayoutType.BROWSING) return;
                        maybeUpdateOverlay(mLatestTab);
                    }
                };

        mActivityTabProvider.addObserver(mTabCallback, NotifyBehavior.NOTIFY_ON_ADD);
        mLayoutManagerSupplier.addObserver(
                mLayoutManagerChangeCallback, NotifyBehavior.NOTIFY_ON_ADD);

        maybeUpdateOverlay(mLatestTab);
    }

    @Override
    public void destroy() {
        mLayoutManagerSupplier.removeObserver(mLayoutManagerChangeCallback);
        if (mLayoutManager != null) {
            mLayoutManager.removeObserver(mLayoutStateObserver);
        }
        mActivityTabProvider.removeObserver(mTabCallback);

        super.destroy();
    }

    private void maybeUpdateOverlay(@Nullable Tab tab) {
        mLatestTab = tab;

        if (mLayoutManager == null) {
            // Before compositor layer is ready, keep the current overlay setting.
            return;
        }

        @LayoutType int currentLayout = mLayoutManager.getActiveLayoutType();
        @LayoutType int nextLayout = mLayoutManager.getNextLayoutType();
        boolean isInBrowsingMode =
                (currentLayout == LayoutType.BROWSING)
                        && (nextLayout == LayoutType.BROWSING || nextLayout == LayoutType.NONE);

        if (isInBrowsingMode) {
            setIsUsingOverlay(mLatestTab != null && mLatestTab.isIncognitoBranded());
        } else {
            // During any layout transition, the overlay has to be turned off so layout can pickup
            // correct colors for both mode.
            setIsUsingOverlay(false);
        }
    }

    private void updateLayoutManager(
            @Nullable LayoutManagerImpl newInstance, @Nullable LayoutManagerImpl oldInstance) {
        if (oldInstance != null) {
            oldInstance.removeObserver(mLayoutStateObserver);
        }
        mLayoutManager = newInstance;
        if (newInstance != null) {
            newInstance.addObserver(mLayoutStateObserver);
        }
        maybeUpdateOverlay(mLatestTab);
    }
}
