// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Rect;
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
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_management.RecyclerViewPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherCustomViewManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator of the single tab tab switcher. */
public class SingleTabSwitcherCoordinator implements TabSwitcher {
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private final SingleTabSwitcherMediator mMediator;
    private final SingleTabSwitcherOnTabletMediator mMediatorOnTablet;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private final TabSwitcher.TabListDelegate mTabListDelegate;
    private final TabModelSelector mTabModelSelector;
    private ViewGroup mContainer;
    private boolean mIsTablet;
    private TabObserver mLastActiveTabObserver;
    private Tab mLastActiveTab;

    @Nullable
    private final Runnable mSnapshotParentViewRunnable;

    public SingleTabSwitcherCoordinator(@NonNull Activity activity, @NonNull ViewGroup container,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull TabModelSelector tabModelSelector, boolean isTablet,
            boolean isScrollableMvtEnabled, Tab mostRecentTab,
            @Nullable Runnable singleTabCardClickedCallback,
            @Nullable Runnable snapshotParentViewRunnable,
            @Nullable TabContentManager tabContentManager) {
        mTabModelSelector = tabModelSelector;
        mIsTablet = isTablet;
        mLastActiveTab = mostRecentTab;
        mSnapshotParentViewRunnable = snapshotParentViewRunnable;
        boolean isSurfacePolishEnabled = isSurfacePolishEnabled();
        PropertyModel propertyModel = new PropertyModel(SingleTabViewProperties.ALL_KEYS);
        int layoutId = isSurfacePolishEnabled ? R.layout.single_tab_module_layout
                                              : R.layout.single_tab_view_layout;
        SingleTabView singleTabView =
                (SingleTabView) LayoutInflater.from(activity).inflate(layoutId, container, false);
        mContainer = container;
        mContainer.addView(singleTabView);
        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(
                propertyModel, singleTabView, SingleTabViewBinder::bind);
        mTabListFaviconProvider = new TabListFaviconProvider(activity, false,
                isSurfacePolishEnabled ? R.dimen.favicon_corner_radius_polished
                                       : R.dimen.default_favicon_corner_radius);
        if (!mIsTablet) {
            mMediator = new SingleTabSwitcherMediator(activity, propertyModel, tabModelSelector,
                    mTabListFaviconProvider, isSurfacePolishEnabled ? tabContentManager : null,
                    isSurfacePolishEnabled);
            mMediatorOnTablet = null;
        } else {
            mMediatorOnTablet = new SingleTabSwitcherOnTabletMediator(activity, propertyModel,
                    activityLifecycleDispatcher, tabModelSelector, mTabListFaviconProvider,
                    mostRecentTab, isScrollableMvtEnabled, singleTabCardClickedCallback,
                    isSurfacePolishEnabled ? tabContentManager : null);
            mMediator = null;
        }
        if (ChromeFeatureList.sInstantStart.isEnabled()) {
            new TabAttributeCache(tabModelSelector);
        }

        // Most of these interfaces should be unused. They are invalid implementations.
        mTabListDelegate = new TabSwitcher.TabListDelegate() {
            @Override
            public int getResourceId() {
                return 0;
            }

            @Override
            public long getLastDirtyTime() {
                assert false : "should not reach here";
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
            public int getSoftCleanupDelayForTesting() {
                assert false : "should not reach here";
                return 0;
            }

            @Override
            public int getCleanupDelayForTesting() {
                assert false : "should not reach here";
                return 0;
            }

            @Override
            @VisibleForTesting
            public int getTabListTopOffset() {
                return 0;
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

        mLastActiveTabObserver = new EmptyTabObserver() {
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
        mTabListFaviconProvider.initWithNative(
                mTabModelSelector.getModel(/*isIncognito=*/false).getProfile());
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
    public boolean onBackPressed() {
        return false;
    }

    @Override
    public int getTabSwitcherTabListModelSize() {
        return 0;
    }

    @Override
    public void setTabSwitcherRecyclerViewPosition(RecyclerViewPosition recyclerViewPosition) {}

    /** @see SingleTabSwitcherOnTabletMediator#setVisibility. */
    void setVisibility(boolean isVisible) {
        if (!mIsTablet) return;

        mMediatorOnTablet.setVisibility(isVisible);
        mContainer.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    /**
     * Shows the single tab module and updates the Tab to track. It is possible to hide the single
     * Tab module if the new tracking Tab is invalid: e.g., a NTP.
     * @param mostRecentTab The most recent Tab to track.
     */
    public void showModule(Tab mostRecentTab) {
        if (!mIsTablet) return;

        showModule(true, mostRecentTab);
    }

    /**
     * Shows the single tab module.
     */
    public void showModule() {
        if (!mIsTablet) return;

        showModule(false, null);
    }

    /**
     * Hides the single tab module.
     */
    public void hide() {
        if (!mIsTablet) return;

        setVisibility(false);
    }

    /**
     * Shows the single tab module.
     * @param shouldUpdateTab Whether to update the tracking Tab of the single Tab module.
     * @param mostRecentTab The most recent Tab to track.
     */
    private void showModule(boolean shouldUpdateTab, Tab mostRecentTab) {
        if (!mIsTablet) return;

        boolean hasTabToTrack = true;
        if (shouldUpdateTab) {
            hasTabToTrack = updateTrackingTab(mostRecentTab);
        }
        setVisibility(hasTabToTrack);
    }
    /**
     * Update the most recent tab to track in the single tab card.
     * @param tabToTrack The tab to track as the most recent tab.
     * @return Whether has a Tab to track. Returns false if the Tab to track is set as null.
     */
    public boolean updateTrackingTab(Tab tabToTrack) {
        assert mIsTablet;
        boolean hasTabToTrack = mMediatorOnTablet.setTab(tabToTrack);
        if (hasTabToTrack && mLastActiveTab == null) {
            mLastActiveTab = tabToTrack;
            beginObserving();
        }
        return hasTabToTrack;
    }

    public void destroy() {
        if (mLastActiveTab != null) {
            mLastActiveTab.removeObserver(mLastActiveTabObserver);
            mLastActiveTab = null;
            mLastActiveTabObserver = null;
        }
        if (mMediatorOnTablet != null) {
            mMediatorOnTablet.destroy();
        }
    }

    @VisibleForTesting
    public boolean isVisible() {
        if (mMediatorOnTablet == null) return false;

        return mMediatorOnTablet.isVisible();
    }

    private boolean isSurfacePolishEnabled() {
        return ChromeFeatureList.sSurfacePolish.isEnabled();
    }
}
