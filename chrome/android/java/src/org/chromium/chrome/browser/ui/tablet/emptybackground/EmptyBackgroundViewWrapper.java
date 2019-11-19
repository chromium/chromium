// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.tablet.emptybackground;

import android.app.Activity;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.ViewStub;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;

import java.util.List;

/**
 * Handler for {@link EmptyBackgroundViewTablet}.
 */
public class EmptyBackgroundViewWrapper {
    private final Activity mActivity;
    private final TabModelSelector mTabModelSelector;
    private final TabCreator mTabCreator;
    private final TabModelObserver mTabModelObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final SnackbarManager mSnackbarManager;

    private final ObservableSupplier<OverviewModeBehavior> mOverviewModeBehaviorSupplier;
    private final Callback<OverviewModeBehavior> mOverviewModeSupplierCallback;
    private @Nullable OverviewModeBehavior mOverviewModeBehavior;

    private EmptyBackgroundViewTablet mBackgroundView;
    private final @Nullable AppMenuHandler mMenuHandler;

    /**
     * Creates a {@link EmptyBackgroundViewWrapper} instance that will lazily inflate.
     * @param selector A {@link TabModelSelector} that will be used to query system state.
     * @param tabCreator A {@link TabCreator} that will be used to open the New Tab Page.
     * @param activity An {@link Activity} that represents a parent of th
     *         {@link android.view.ViewStub}.
     * @param menuHandler A {@link AppMenuHandler} to handle menu touch events.
     * @param snackbarManager The {@link SnackbarManager} to show the undo snackbar when the empty
     *         background is visible.
     * @param overviewModeBehaviorSupplier An {@link ObservableSupplier} for the
     *         {@link OverviewModeBehavior} associated with the containing activity.
     */
    public EmptyBackgroundViewWrapper(TabModelSelector selector, TabCreator tabCreator,
            Activity activity, @Nullable AppMenuHandler menuHandler,
            SnackbarManager snackbarManager,
            ObservableSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier) {
        mActivity = activity;
        mMenuHandler = menuHandler;
        mTabModelSelector = selector;
        mTabCreator = tabCreator;
        mSnackbarManager = snackbarManager;

        mOverviewModeBehaviorSupplier = overviewModeBehaviorSupplier;
        mOverviewModeSupplierCallback =
                overviewModeBehavior -> mOverviewModeBehavior = overviewModeBehavior;
        mOverviewModeBehaviorSupplier.addObserver(mOverviewModeSupplierCallback);

        mTabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didAddTab(Tab tab, @TabLaunchType int type) {
                updateEmptyContainerState();
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                updateEmptyContainerState();
            }

            @Override
            public void didCloseTab(int tabId, boolean incognito) {
                updateEmptyContainerState();
            }

            @Override
            public void tabPendingClosure(Tab tab) {
                updateEmptyContainerState();
            }

            @Override
            public void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs) {
                updateEmptyContainerState();
            }

            @Override
            public void tabRemoved(Tab tab) {
                updateEmptyContainerState();
            }
        };
        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                updateEmptyContainerState();
            }
        };
    }

    /**
     * Called when the containing activity is being destroyed.
     */
    public void destroy() {
        mOverviewModeBehaviorSupplier.removeObserver(mOverviewModeSupplierCallback);
    }

    /**
     * Initialize the wrapper to listen for the proper notifications.
     */
    public void initialize() {
        for (TabModel model : mTabModelSelector.getModels()) model.addObserver(mTabModelObserver);
        mTabModelSelector.addObserver(mTabModelSelectorObserver);
    }

    /**
     * Unregister all dependencies and listeners.
     */
    public void uninitialize() {
        for (TabModel model : mTabModelSelector.getModels()) {
            model.removeObserver(mTabModelObserver);
        }
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
    }

    private void inflateViewIfNecessary() {
        if (mBackgroundView != null) return;

        mBackgroundView = (EmptyBackgroundViewTablet) ((ViewStub) mActivity.findViewById(
                                                               R.id.empty_container_stub))
                                  .inflate();
        mBackgroundView.setTabModelSelector(mTabModelSelector);
        mBackgroundView.setTabCreator(mTabCreator);
        if (mMenuHandler != null) mBackgroundView.setMenuOnTouchListener(mMenuHandler);
        mBackgroundView.addOnAttachStateChangeListener(new OnAttachStateChangeListener() {
            @Override
            public void onViewDetachedFromWindow(View v) {
                uninitialize();
            }

            @Override
            public void onViewAttachedToWindow(View v) {}
        });
    }

    private void updateEmptyContainerState() {
        boolean showEmptyBackground = shouldShowEmptyContainer();
        if (showEmptyBackground) inflateViewIfNecessary();

        if (mBackgroundView != null) {
            mBackgroundView.setEmptyContainerState(showEmptyBackground);
            mSnackbarManager.overrideParent(showEmptyBackground ? mBackgroundView : null);
        }
    }

    private boolean shouldShowEmptyContainer() {
        TabModel model = mTabModelSelector.getModel(false);
        if (model == null) return false;

        boolean isIncognitoEmpty = mTabModelSelector.getModel(true).getCount() == 0;
        boolean incognitoSelected = mTabModelSelector.isIncognitoSelected();

        // Only show the empty container if:
        // 1. There are no tabs in the normal TabModel AND
        // 2. Overview mode is not showing AND
        // 3. We're in the normal TabModel OR there are no tabs present in either model
        return model.getCount() == 0
                && (mOverviewModeBehavior == null || !mOverviewModeBehavior.overviewVisible())
                && (!incognitoSelected || isIncognitoEmpty);
    }
}
