// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.single_tab;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.magic_stack.ModuleProvider;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator of the single tab tab switcher. */
public class SingleTabSwitcherCoordinator implements ModuleProvider {

    private final SingleTabSwitcherOnNtpMediator mMediatorOnNtp;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private TabObserver mLastActiveTabObserver;
    private Tab mLastActiveTab;

    /** Null if created by {@link org.chromium.chrome.browser.magic_stack.ModuleProviderBuilder} */
    @Nullable private final ViewGroup mContainer;

    @Nullable private final Runnable mSnapshotParentViewRunnable;
    @Nullable private final ModuleDelegate mModuleDelegate;

    public SingleTabSwitcherCoordinator(
            @NonNull Activity activity,
            @NonNull ViewGroup container,
            @NonNull TabModelSelector tabModelSelector,
            boolean isTablet,
            Tab mostRecentTab,
            @Nullable Callback<Integer> singleTabCardClickedCallback,
            @Nullable Runnable seeMoreLinkClickedCallback,
            @Nullable Runnable snapshotParentViewRunnable,
            @Nullable TabContentManager tabContentManager,
            @Nullable UiConfig uiConfig,
            @Nullable ModuleDelegate moduleDelegate) {
        mLastActiveTab = mostRecentTab;
        mSnapshotParentViewRunnable = snapshotParentViewRunnable;
        PropertyModel propertyModel = new PropertyModel(SingleTabViewProperties.ALL_KEYS);
        mContainer = container;
        mModuleDelegate = moduleDelegate;

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
                        org.chromium.chrome.browser.tab_ui.R.dimen
                                .favicon_corner_radius_for_single_tab_switcher);
        mMediatorOnNtp =
                new SingleTabSwitcherOnNtpMediator(
                        activity,
                        propertyModel,
                        tabModelSelector,
                        mTabListFaviconProvider,
                        mostRecentTab,
                        singleTabCardClickedCallback,
                        seeMoreLinkClickedCallback,
                        tabContentManager,
                        isTablet ? uiConfig : null,
                        isTablet,
                        moduleDelegate);

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
                            if (mModuleDelegate != null) {
                                mModuleDelegate.removeModule(ModuleType.SINGLE_TAB);
                            }
                        }
                    }
                };
        mLastActiveTab.addObserver(mLastActiveTabObserver);
    }

    /**
     * @see SingleTabSwitcherOnNtpMediator#setVisibility.
     */
    void setVisibility(boolean isVisible) {
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
        setVisibility(false);
    }

    /**
     * Shows the single tab module.
     *
     * @param shouldUpdateTab Whether to update the tracking Tab of the single Tab module.
     * @param mostRecentTab The most recent Tab to track.
     */
    private void show(boolean shouldUpdateTab, Tab mostRecentTab) {
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
        boolean hasTabToTrack = mMediatorOnNtp.setTab(tabToTrack);
        if (hasTabToTrack && mLastActiveTab == null) {
            mLastActiveTab = tabToTrack;
            beginObserving();
        }
        return hasTabToTrack;
    }

    /** Returns the layout resource id for the single tab card. */
    public static int getModuleLayoutId() {
        return R.layout.single_tab_module_layout;
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
        show(false, null);
    }

    @Override
    public void hideModule() {
        mMediatorOnNtp.destroy();
    }

    @Override
    public int getModuleType() {
        return ModuleType.SINGLE_TAB;
    }

    @Override
    public void onContextMenuCreated() {}

    @Override
    public String getModuleContextMenuHideText(Context context) {
        return context.getResources()
                .getQuantityString(R.plurals.home_modules_context_menu_hide_tab, 1);
    }

    @VisibleForTesting
    public boolean isVisible() {
        if (mMediatorOnNtp == null) return false;

        return mMediatorOnNtp.isVisible();
    }
}
