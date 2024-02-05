// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.util.Size;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_management.RecyclerViewPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherCustomViewManager;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator of the single tab tab switcher. */
public class SingleTabSwitcherCoordinator implements TabSwitcher, ModuleProvider {

    private final SingleTabSwitcherMediator mMediator;
    private final SingleTabSwitcherOnNtpMediator mMediatorOnNtp;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private final TabSwitcher.TabListDelegate mTabListDelegate;
    private final boolean mIsSurfacePolishEnabled;
    private boolean mIsShownOnNtp;
    private TabObserver mLastActiveTabObserver;
    private Tab mLastActiveTab;

    /** Null if created by {@link org.chromium.chrome.browser.magic_stack.ModuleProviderBuilder} */
    @Nullable private final ViewGroup mContainer;

    @Nullable private final Runnable mSnapshotParentViewRunnable;

    public SingleTabSwitcherCoordinator(
            @NonNull Activity activity,
            @NonNull ViewGroup container,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull TabModelSelector tabModelSelector,
            boolean isShownOnNtp,
            boolean isTablet,
            boolean isScrollableMvtEnabled,
            Tab mostRecentTab,
            @Nullable Callback<Integer> singleTabCardClickedCallback,
            @Nullable Runnable snapshotParentViewRunnable,
            @Nullable TabContentManager tabContentManager,
            @Nullable UiConfig uiConfig,
            @Nullable ModuleDelegate moduleDelegate) {
        mIsShownOnNtp = isShownOnNtp;
        mLastActiveTab = mostRecentTab;
        mSnapshotParentViewRunnable = snapshotParentViewRunnable;
        mIsSurfacePolishEnabled = isSurfacePolishEnabled();
        PropertyModel propertyModel = new PropertyModel(SingleTabViewProperties.ALL_KEYS);
        mContainer = container;

        if (moduleDelegate == null) {
            SingleTabView singleTabView =
                    (SingleTabView)
                            LayoutInflater.from(activity)
                                    .inflate(getModuleLayoutId(), container, false);
            mContainer.addView(singleTabView);
            PropertyModelChangeProcessor.create(
                    propertyModel, singleTabView, SingleTabViewBinder::bind);
        }
        mTabListFaviconProvider =
                new TabListFaviconProvider(
                        activity,
                        false,
                        mIsSurfacePolishEnabled
                                ? R.dimen.favicon_corner_radius_polished
                                : R.dimen.default_favicon_corner_radius);
        if (!mIsShownOnNtp) {
            mMediator =
                    new SingleTabSwitcherMediator(
                            activity,
                            propertyModel,
                            tabModelSelector,
                            mTabListFaviconProvider,
                            mIsSurfacePolishEnabled ? tabContentManager : null,
                            singleTabCardClickedCallback,
                            mIsSurfacePolishEnabled,
                            moduleDelegate);
            mMediatorOnNtp = null;
        } else {
            mMediatorOnNtp =
                    new SingleTabSwitcherOnNtpMediator(
                            activity,
                            propertyModel,
                            activityLifecycleDispatcher,
                            tabModelSelector,
                            mTabListFaviconProvider,
                            mostRecentTab,
                            isScrollableMvtEnabled,
                            singleTabCardClickedCallback,
                            mIsSurfacePolishEnabled ? tabContentManager : null,
                            mIsSurfacePolishEnabled && isTablet ? uiConfig : null,
                            isTablet,
                            moduleDelegate);
            mMediator = null;
        }
        if (ChromeFeatureList.sInstantStart.isEnabled()) {
            new TabAttributeCache(tabModelSelector);
        }

        // Most of these interfaces should be unused. They are invalid implementations.
        mTabListDelegate =
                new TabSwitcher.TabListDelegate() {
                    @Override
                    public int getResourceId() {
                        return 0;
                    }

                    @Override
                    public void setBitmapCallbackForTesting(Callback<Bitmap> callback) {
                        assert false : "should not reach here";
                    }

                    @Override
                    public int getBitmapFetchCountForTesting() {
                        assert false : "should not reach here";
                        return 0;
                    }

                    @Override
                    @VisibleForTesting
                    public int getTabListTopOffset() {
                        return 0;
                    }

                    @Override
                    @VisibleForTesting
                    public Rect getRecyclerViewLocation() {
                        return null;
                    }

                    @Override
                    public int getListModeForTesting() {
                        assert false : "should not reach here";
                        return 0;
                    }

                    @Override
                    public void prepareTabGridView() {
                        assert false : "should not reach here";
                    }

                    @Override
                    public boolean prepareTabSwitcherView() {
                        return true;
                    }

                    @Override
                    public void postHiding() {}

                    @Override
                    public Rect getThumbnailLocationOfCurrentTab() {
                        assert false : "should not reach here";
                        return null;
                    }

                    @Override
                    public Size getThumbnailSize() {
                        assert false : "should not reach here";
                        return null;
                    }

                    @Override
                    public void runAnimationOnNextLayout(Runnable r) {
                        assert false : "should not reach here";
                    }
                };

        if (mLastActiveTab != null) {
            beginObserving();
        }
    }

    private void beginObserving() {
        if (mLastActiveTab == null) return;

        mLastActiveTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onClosingStateChanged(Tab tab, boolean closing) {
                        if (closing) {
                            updateTrackingTab(null);
                            setVisibility(false);
                            mLastActiveTab.removeObserver(mLastActiveTabObserver);
                            mLastActiveTab = null;
                            mLastActiveTabObserver = null;
                            if (mSnapshotParentViewRunnable != null) {
                                mSnapshotParentViewRunnable.run();
                            }
                        }
                    }
                };
        mLastActiveTab.addObserver(mLastActiveTabObserver);
    }

    // TabSwitcher implementation.
    @Override
    public void setOnTabSelectingListener(OnTabSelectingListener listener) {
        assert mMediator != null;
        mMediator.setOnTabSelectingListener(listener);
    }

    @Override
    public void initWithNative() {
        if (mMediator != null) {
            mMediator.initWithNative();
        }
    }

    @Override
    public Controller getController() {
        return mMediator;
    }

    @Override
    public TabListDelegate getTabListDelegate() {
        return mTabListDelegate;
    }

    @Override
    public Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        return null;
    }

    @Override
    public @Nullable TabSwitcherCustomViewManager getTabSwitcherCustomViewManager() {
        return null;
    }

    @Override
    public int getTabSwitcherTabListModelSize() {
        return 0;
    }

    @Override
    public void setTabSwitcherRecyclerViewPosition(RecyclerViewPosition recyclerViewPosition) {}

    /**
     * @see SingleTabSwitcherOnNtpMediator#setVisibility.
     */
    void setVisibility(boolean isVisible) {
        if (!mIsShownOnNtp) return;

        mMediatorOnNtp.setVisibility(isVisible);
        if (mContainer != null) {
            mContainer.setVisibility(isVisible ? View.VISIBLE : View.GONE);
        }
    }

    /**
     * Shows the single tab module and updates the Tab to track. It is possible to hide the single
     * Tab module if the new tracking Tab is invalid: e.g., a NTP.
     *
     * @param mostRecentTab The most recent Tab to track.
     */
    public void show(Tab mostRecentTab) {
        show(true, mostRecentTab);
    }

    /** Hides the single tab module. */
    public void hide() {
        if (!mIsShownOnNtp) return;

        setVisibility(false);
    }

    /**
     * Shows the single tab module.
     *
     * @param shouldUpdateTab Whether to update the tracking Tab of the single Tab module.
     * @param mostRecentTab The most recent Tab to track.
     */
    private void show(boolean shouldUpdateTab, Tab mostRecentTab) {
        if (!mIsShownOnNtp) return;

        boolean hasTabToTrack = true;
        if (shouldUpdateTab) {
            hasTabToTrack = updateTrackingTab(mostRecentTab);
        }
        setVisibility(hasTabToTrack);
    }

    /**
     * Update the most recent tab to track in the single tab card.
     *
     * @param tabToTrack The tab to track as the most recent tab.
     * @return Whether has a Tab to track. Returns false if the Tab to track is set as null.
     */
    public boolean updateTrackingTab(Tab tabToTrack) {
        assert mIsShownOnNtp;
        boolean hasTabToTrack = mMediatorOnNtp.setTab(tabToTrack);
        if (hasTabToTrack && mLastActiveTab == null) {
            mLastActiveTab = tabToTrack;
            beginObserving();
        }
        return hasTabToTrack;
    }

    /** Returns the layout resource id for the single tab card. */
    public static int getModuleLayoutId() {
        return ChromeFeatureList.sSurfacePolish.isEnabled()
                ? R.layout.single_tab_module_layout
                : R.layout.single_tab_view_layout;
    }

    /**
     * Sets a {@link StartSurface.OnTabSelectingListener}. This should be only used when the single
     * tab card is shown on the Start surface.
     */
    public void setOnModuleSelectedListener(StartSurface.OnTabSelectingListener observer) {
        mMediator.setOnTabSelectingListener(observer);
    }

    public void destroy() {
        if (mLastActiveTab != null) {
            mLastActiveTab.removeObserver(mLastActiveTabObserver);
            mLastActiveTab = null;
            mLastActiveTabObserver = null;
        }
        if (mMediatorOnNtp != null) {
            mMediatorOnNtp.destroy();
        }
    }

    // ModuleProvider implementation.

    @Override
    public void showModule() {
        if (mMediator != null) {
            mMediator.showModule();
        } else {
            show(false, null);
        }
    }

    @Override
    public void hideModule() {
        if (mMediator != null) {
            mMediator.hideTabSwitcherView(false);
        } else {
            mMediatorOnNtp.destroy();
        }
    }

    @Override
    public int getModuleType() {
        return ModuleType.SINGLE_TAB;
    }

    @Override
    public void onContextMenuCreated() {}

    @Override
    public String getModuleTitle(Context context) {
        return context.getString(org.chromium.chrome.tab_ui.R.string.single_tab_module_title);
    }

    @VisibleForTesting
    public boolean isVisible() {
        if (mMediatorOnNtp == null) return false;

        return mMediatorOnNtp.isVisible();
    }

    private boolean isSurfacePolishEnabled() {
        return ChromeFeatureList.sSurfacePolish.isEnabled();
    }
}
