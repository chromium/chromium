// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_BAR_HEIGHT;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.tasks.TasksSurface;
import org.chromium.chrome.browser.tasks.TasksSurfaceProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.features.start_surface.StartSurfaceMediator.SurfaceMode;
import org.chromium.chrome.start_surface.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * Root coordinator that is responsible for showing start surfaces, like a grid of Tabs, explore
 * surface and the bottom bar to switch between them.
 */
public class StartSurfaceCoordinator implements StartSurface {
    private final ChromeActivity mActivity;
    private final StartSurfaceMediator mStartSurfaceMediator;
    private final @SurfaceMode int mSurfaceMode;

    // Non-null in SurfaceMode.TASKS_ONLY, SurfaceMode.TWO_PANES and SurfaceMode.SINGLE_PANE modes.
    @Nullable
    private TasksSurface mTasksSurface;

    // Non-null in SurfaceMode.TASKS_ONLY, SurfaceMode.TWO_PANES and SurfaceMode.SINGLE_PANE modes.
    @Nullable
    private PropertyModelChangeProcessor mTasksSurfacePropertyModelChangeProcessor;

    // Non-null in SurfaceMode.SINGLE_PANE mode to show more tabs.
    @Nullable
    private TasksSurface mSecondaryTasksSurface;

    // Non-null in SurfaceMode.SINGLE_PANE mode to show more tabs.
    @Nullable
    private PropertyModelChangeProcessor mSecondaryTasksSurfacePropertyModelChangeProcessor;

    // Non-null in SurfaceMode.NO_START_SURFACE to show the tabs.
    @Nullable
    private TabSwitcher mTabSwitcher;

    // Non-null in SurfaceMode.TWO_PANES mode.
    @Nullable
    private BottomBarCoordinator mBottomBarCoordinator;

    // Non-null in SurfaceMode.TWO_PANES and SurfaceMode.SINGLE_PANE modes.
    @Nullable
    private ExploreSurfaceCoordinator mExploreSurfaceCoordinator;

    // Non-null in SurfaceMode.TWO_PANES and SurfaceMode.SINGLE_PANE modes.
    // TODO(crbug.com/982018): Get rid of this reference since the mediator keeps a reference to it.
    @Nullable
    private PropertyModel mPropertyModel;

    // Used to remember TabSwitcher.OnTabSelectingListener in SurfaceMode.SINGLE_PANE mode for more
    // tabs surface if necessary.
    @Nullable
    private TabSwitcher.OnTabSelectingListener mOnTabSelectingListener;

    public StartSurfaceCoordinator(ChromeActivity activity) {
        mActivity = activity;
        mSurfaceMode = computeSurfaceMode();

        if (mSurfaceMode == SurfaceMode.NO_START_SURFACE) {
            // Create Tab switcher directly to save one layer in the view hierarchy.
            mTabSwitcher = TabManagementModuleProvider.getDelegate().createGridTabSwitcher(
                    mActivity, mActivity.getCompositorViewHolder());
        } else {
            createAndSetStartSurface();
        }

        TabSwitcher.Controller controller =
                mTabSwitcher != null ? mTabSwitcher.getController() : mTasksSurface.getController();
        mStartSurfaceMediator = new StartSurfaceMediator(controller,
                mActivity.getTabModelSelector(), mPropertyModel,
                mExploreSurfaceCoordinator == null
                        ? null
                        : mExploreSurfaceCoordinator.getFeedSurfaceCreator(),
                mSurfaceMode == SurfaceMode.SINGLE_PANE ? this::initializeSecondaryTasksSurface
                                                        : null,
                mSurfaceMode,
                mSurfaceMode != SurfaceMode.NO_START_SURFACE
                        ? mActivity.getToolbarManager().getFakeboxDelegate()
                        : null,
                mActivity.getNightModeStateProvider());
    }

    // Implements StartSurface.
    @Override
    public void setStateChangeObserver(StartSurface.StateObserver observer) {
        mStartSurfaceMediator.setStateChangeObserver(observer);
    }

    @Override
    public void setOnTabSelectingListener(StartSurface.OnTabSelectingListener listener) {
        if (mTasksSurface != null) {
            mTasksSurface.setOnTabSelectingListener(listener);
        } else {
            mTabSwitcher.setOnTabSelectingListener(listener);
        }

        // Set OnTabSelectingListener to the more tabs tasks surface as well if it has been
        // instantiated, otherwise remember it for the future instantiation.
        if (mSurfaceMode == SurfaceMode.SINGLE_PANE) {
            if (mSecondaryTasksSurface == null) {
                mOnTabSelectingListener = listener;
            } else {
                mSecondaryTasksSurface.setOnTabSelectingListener(listener);
            }
        }
    }

    @Override
    public Controller getController() {
        return mStartSurfaceMediator;
    }

    @Override
    public TabSwitcher.TabListDelegate getTabListDelegate() {
        if (mTasksSurface != null) {
            return mTasksSurface.getTabListDelegate();
        }

        return mTabSwitcher.getTabListDelegate();
    }

    @Override
    public TabSwitcher.TabDialogDelegation getTabDialogDelegate() {
        return mTabSwitcher.getTabGridDialogDelegation();
    }

    private @SurfaceMode int computeSurfaceMode() {
        // Check the cached flag before getting the parameter to be consistent with the other
        // places. Note that the cached flag may have been set before native initialization.
        if (!FeatureUtilities.isStartSurfaceEnabled()) {
            return SurfaceMode.NO_START_SURFACE;
        }

        String feature = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.START_SURFACE_ANDROID, "start_surface_variation");

        if (feature.equals("twopanes")) {
            // Do not enable two panes when the bottom bar is enabled since it will
            // overlap the two panes' bottom bar.
            return FeatureUtilities.isBottomToolbarEnabled() ? SurfaceMode.SINGLE_PANE
                                                             : SurfaceMode.TWO_PANES;
        }

        if (feature.equals("single")) return SurfaceMode.SINGLE_PANE;

        if (feature.equals("tasksonly")) return SurfaceMode.TASKS_ONLY;

        // Default to SurfaceMode.TASKS_ONLY. This could happen when the start surface has been
        // changed from enabled to disabled in native side, but the cached flag has not been updated
        // yet, so FeatureUtilities.isStartSurfaceEnabled() above returns true.
        // TODO(crbug.com/1016548): Remember the last surface mode so as to default to it.
        return SurfaceMode.TASKS_ONLY;
    }

    private void createAndSetStartSurface() {
        ArrayList<PropertyKey> allProperties =
                new ArrayList<>(Arrays.asList(TasksSurfaceProperties.ALL_KEYS));
        allProperties.addAll(Arrays.asList(StartSurfaceProperties.ALL_KEYS));
        mPropertyModel = new PropertyModel(allProperties);
        mPropertyModel.set(TOP_BAR_HEIGHT,
                mActivity.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow));

        mTasksSurface = TabManagementModuleProvider.getDelegate().createTasksSurface(mActivity,
                mPropertyModel, mActivity.getToolbarManager().getFakeboxDelegate(),
                mSurfaceMode == SurfaceMode.SINGLE_PANE);

        mTasksSurfacePropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mPropertyModel,
                        new TasksSurfaceViewBinder.ViewHolder(
                                mActivity.getCompositorViewHolder(), mTasksSurface.getView()),
                        TasksSurfaceViewBinder::bind);

        // There is nothing else to do for SurfaceMode.TASKS_ONLY.
        if (mSurfaceMode == SurfaceMode.TASKS_ONLY) {
            return;
        }

        if (mSurfaceMode == SurfaceMode.TWO_PANES) {
            mBottomBarCoordinator = new BottomBarCoordinator(
                    mActivity, mActivity.getCompositorViewHolder(), mPropertyModel);
        }

        mExploreSurfaceCoordinator = new ExploreSurfaceCoordinator(mActivity,
                mSurfaceMode == SurfaceMode.SINGLE_PANE ? mTasksSurface.getBodyViewContainer()
                                                        : mActivity.getCompositorViewHolder(),
                mPropertyModel, mSurfaceMode == SurfaceMode.SINGLE_PANE);
    }

    private TabSwitcher.Controller initializeSecondaryTasksSurface() {
        assert mSurfaceMode == SurfaceMode.SINGLE_PANE;
        assert mSecondaryTasksSurface == null;

        PropertyModel propertyModel = new PropertyModel(TasksSurfaceProperties.ALL_KEYS);
        mStartSurfaceMediator.setSecondaryTasksSurfacePropertyModel(propertyModel);
        mSecondaryTasksSurface =
                TabManagementModuleProvider.getDelegate().createTasksSurface(mActivity,
                        propertyModel, mActivity.getToolbarManager().getFakeboxDelegate(), false);
        mSecondaryTasksSurfacePropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mPropertyModel,
                        new TasksSurfaceViewBinder.ViewHolder(mActivity.getCompositorViewHolder(),
                                mSecondaryTasksSurface.getView()),
                        SecondaryTasksSurfaceViewBinder::bind);
        if (mOnTabSelectingListener != null) {
            mSecondaryTasksSurface.setOnTabSelectingListener(mOnTabSelectingListener);
            mOnTabSelectingListener = null;
        }
        return mSecondaryTasksSurface.getController();
    }
}
