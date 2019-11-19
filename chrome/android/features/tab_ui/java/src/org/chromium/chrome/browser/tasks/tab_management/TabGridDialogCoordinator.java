// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Rect;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * A coordinator for TabGridDialog component. Manages the communication with
 * {@link TabListCoordinator} as well as the life-cycle of shared component
 * objects.
 */
public class TabGridDialogCoordinator implements TabGridDialogMediator.DialogController {
    private final String mComponentName;
    private final TabListCoordinator mTabListCoordinator;
    private final TabGridDialogMediator mMediator;
    private final PropertyModel mToolbarPropertyModel;
    private final TabGridPanelToolbarCoordinator mToolbarCoordinator;
    private final TabSelectionEditorCoordinator mTabSelectionEditorCoordinator;
    private final TabGridDialogParent mParentLayout;

    TabGridDialogCoordinator(Context context, TabModelSelector tabModelSelector,
            TabContentManager tabContentManager, TabCreatorManager tabCreatorManager,
            ViewGroup containerView, TabSwitcherMediator.ResetHandler resetHandler,
            TabListMediator.GridCardOnClickListenerProvider gridCardOnClickListenerProvider,
            TabGridDialogMediator.AnimationSourceViewProvider animationSourceViewProvider,
            TabGroupTitleEditor tabGroupTitleEditor) {
        mComponentName = animationSourceViewProvider == null ? "TabGridDialogFromStrip"
                                                             : "TabGridDialogInSwitcher";

        mToolbarPropertyModel = new PropertyModel(TabGridPanelProperties.ALL_KEYS);

        mParentLayout = new TabGridDialogParent(context, containerView);

        TabSelectionEditorCoordinator.TabSelectionEditorController controller = null;
        if (FeatureUtilities.isTabGroupsAndroidContinuationEnabled()) {
            mTabSelectionEditorCoordinator = new TabSelectionEditorCoordinator(
                    context, containerView, tabModelSelector, tabContentManager, mParentLayout);

            controller = mTabSelectionEditorCoordinator.getController();
        } else {
            mTabSelectionEditorCoordinator = null;
        }

        mMediator = new TabGridDialogMediator(context, this, mToolbarPropertyModel,
                tabModelSelector, tabCreatorManager, resetHandler, animationSourceViewProvider,
                controller, tabGroupTitleEditor, mComponentName);

        mTabListCoordinator = new TabListCoordinator(TabListCoordinator.TabListMode.GRID, context,
                tabModelSelector, tabContentManager::getTabThumbnailWithCallback, null, false, null,
                gridCardOnClickListenerProvider, mMediator.getTabGridDialogHandler(),
                TabProperties.UiType.CLOSABLE, null, containerView, null, false, mComponentName);

        TabListRecyclerView recyclerView = mTabListCoordinator.getContainerView();
        mToolbarCoordinator = new TabGridPanelToolbarCoordinator(
                context, recyclerView, mToolbarPropertyModel, mParentLayout);
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        mTabListCoordinator.destroy();
        mMediator.destroy();
        mToolbarCoordinator.destroy();
        mParentLayout.destroy();
        if (mTabSelectionEditorCoordinator != null) {
            mTabSelectionEditorCoordinator.destroy();
        }
        if (mToolbarCoordinator != null) {
            mToolbarCoordinator.destroy();
        }
    }

    boolean isVisible() {
        return mMediator.isVisible();
    }

    @NonNull
    Rect getGlobalLocationOfCurrentThumbnail() {
        mTabListCoordinator.updateThumbnailLocation();
        Rect thumbnail = mTabListCoordinator.getThumbnailLocationOfCurrentTab();
        Rect recyclerViewLocation = mTabListCoordinator.getRecyclerViewLocation();
        thumbnail.offset(recyclerViewLocation.left, recyclerViewLocation.top);
        return thumbnail;
    }

    TabGridDialogMediator.DialogController getDialogController() {
        return this;
    }

    @Override
    public void resetWithListOfTabs(@Nullable List<Tab> tabs) {
        mTabListCoordinator.resetWithListOfTabs(tabs);
        mMediator.onReset(tabs);
    }

    @Override
    public void hideDialog(boolean showAnimation) {
        mMediator.hideDialog(showAnimation);
    }

    @Override
    public boolean handleBackPressed() {
        if (!isVisible()) return false;
        mMediator.hideDialog(true);
        RecordUserAction.record("TabGridDialog.Exit");
        return true;
    }
}
