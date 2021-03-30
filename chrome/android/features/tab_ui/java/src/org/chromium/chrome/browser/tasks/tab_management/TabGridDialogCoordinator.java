// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

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
    private final PropertyModel mModel;
    private final PropertyModelChangeProcessor mModelChangeProcessor;
    private TabSelectionEditorCoordinator mTabSelectionEditorCoordinator;
    private ViewGroup mContainerView;
    private TabGridDialogView mDialogView;
    private boolean mIsInitialized;

    TabGridDialogCoordinator(Context context, TabModelSelector tabModelSelector,
            TabContentManager tabContentManager, TabCreatorManager tabCreatorManager,
            ViewGroup containerView, TabSwitcherMediator.ResetHandler resetHandler,
            TabListMediator.GridCardOnClickListenerProvider gridCardOnClickListenerProvider,
            TabGridDialogMediator.AnimationSourceViewProvider animationSourceViewProvider,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            ScrimCoordinator scrimCoordinator) {
        mComponentName = animationSourceViewProvider == null ? "TabGridDialogFromStrip"
                                                             : "TabGridDialogInSwitcher";

        mModel = new PropertyModel(TabGridPanelProperties.ALL_KEYS);
        mContainerView = containerView;

        mDialogView = containerView.findViewById(R.id.dialog_parent_view);
        if (mDialogView == null) {
            LayoutInflater.from(context).inflate(
                    R.layout.tab_grid_dialog_layout, containerView, true);
            mDialogView = containerView.findViewById(R.id.dialog_parent_view);
            mDialogView.setupScrimCoordinator(scrimCoordinator);
        }
        Activity activity = (Activity) context;
        SnackbarManager snackbarManager =
                new SnackbarManager(activity, mDialogView.getSnackBarContainer(), null);

        mMediator = new TabGridDialogMediator(context, this, mModel, tabModelSelector,
                tabCreatorManager, resetHandler, animationSourceViewProvider, shareDelegateSupplier,
                snackbarManager, mComponentName);

        // TODO(crbug.com/1031349) : Remove the inline mode logic here, make the constructor to take
        // in a mode parameter instead.
        mTabListCoordinator = new TabListCoordinator(
                TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled()
                                && SysUtils.isLowEndDevice()
                        ? TabListCoordinator.TabListMode.LIST
                        : TabListCoordinator.TabListMode.GRID,
                context, tabModelSelector, tabContentManager::getTabThumbnailWithCallback, null,
                false, gridCardOnClickListenerProvider, mMediator.getTabGridDialogHandler(),
                TabProperties.UiType.CLOSABLE, null, null, containerView, false, mComponentName);
        TabListRecyclerView recyclerView = mTabListCoordinator.getContainerView();

        TabGroupUiToolbarView toolbarView =
                (TabGroupUiToolbarView) LayoutInflater.from(context).inflate(
                        R.layout.bottom_tab_grid_toolbar, recyclerView, false);
        toolbarView.setupDialogToolbarLayout();
        if (!TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled()) {
            toolbarView.hideTabGroupsContinuationWidgets();
        }
        mModelChangeProcessor = PropertyModelChangeProcessor.create(mModel,
                new TabGridPanelViewBinder.ViewHolder(toolbarView, recyclerView, mDialogView),
                TabGridPanelViewBinder::bind);
    }

    public void initWithNative(Context context, TabModelSelector tabModelSelector,
            TabContentManager tabContentManager, TabGroupTitleEditor tabGroupTitleEditor) {
        if (mIsInitialized) return;

        TabSelectionEditorCoordinator.TabSelectionEditorController controller = null;
        if (TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled()) {
            @TabListCoordinator.TabListMode
            int mode = SysUtils.isLowEndDevice() ? TabListCoordinator.TabListMode.LIST
                                                 : TabListCoordinator.TabListMode.GRID;
            mTabSelectionEditorCoordinator = new TabSelectionEditorCoordinator(context,
                    mDialogView.findViewById(R.id.dialog_container_view), tabModelSelector,
                    tabContentManager, mode);

            controller = mTabSelectionEditorCoordinator.getController();
        } else {
            mTabSelectionEditorCoordinator = null;
        }

        mMediator.initWithNative(controller, tabGroupTitleEditor);
        mTabListCoordinator.initWithNative(null);
    }
    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        mTabListCoordinator.destroy();
        mMediator.destroy();
        mModelChangeProcessor.destroy();
        if (mTabSelectionEditorCoordinator != null) {
            mTabSelectionEditorCoordinator.destroy();
        }
    }

    @Override
    public boolean isVisible() {
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
