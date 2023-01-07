// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.tablet.emptybackground;

import android.app.Activity;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.ViewStub;

import androidx.annotation.Nullable;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

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

    CallbackController mCallbackController = new CallbackController();
    private @Nullable LayoutStateProvider mLayoutStateProvider;

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
     * @param layoutStateProviderSupplier An {@link ObservableSupplier} for the
     *         {@link LayoutManager} associated with the containing activity.
     */
    public EmptyBackgroundViewWrapper(TabModelSelector selector, TabCreator tabCreator,
            Activity activity, @Nullable AppMenuHandler menuHandler,
            SnackbarManager snackbarManager,
            ObservableSupplier<LayoutManager> layoutStateProviderSupplier) {
        mActivity = activity;
        mMenuHandler = menuHandler;
        mTabModelSelector = selector;
        mTabCreator = tabCreator;
        mSnackbarManager = snackbarManager;

        layoutStateProviderSupplier.addObserver(mCallbackController.makeCancelable(
                layoutStateProvider -> mLayoutStateProvider = layoutStateProvider));

        mTabModelObserver = new TabModelObserver() {
            @Override
            public void didAddTab(
                    Tab tab, @TabLaunchType int type, @TabCreationState int creationState) {
                updateEmptyContainerState();
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                updateEmptyContainerState();
            }

            @Override
            public void onFinishingTabClosure(Tab tab) {
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
        mTabModelSelectorObserver = new TabModelSelectorObserver() {
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
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }
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
        if (model == null || TabUiFeatureUtilities.isTabletGridTabSwitcherEnabled(mActivity)) {
            return false;
        }

        boolean isIncognitoEmpty = mTabModelSelector.getModel(true).getCount() == 0;
        boolean incognitoSelected = mTabModelSelector.isIncognitoSelected();

        // Only show the empty container if:
        // 1. There are no tabs in the normal TabModel AND
        // 2. Overview mode is not showing AND
        // 3. We're in the normal TabModel OR there are no tabs present in either model
        return model.getCount() == 0
                && (mLayoutStateProvider == null
                        || !mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER))
                && (!incognitoSelected || isIncognitoEmpty);
    }
}
