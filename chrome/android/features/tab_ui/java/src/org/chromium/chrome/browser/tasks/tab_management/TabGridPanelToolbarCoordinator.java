// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for the toolbar component that will be shown on top of the tab
 * grid components, used in {@link TabGridDialogCoordinator}.
 */
class TabGridPanelToolbarCoordinator implements Destroyable {
    private final TabGroupUiToolbarView mToolbarView;
    private final PropertyModelChangeProcessor mModelChangeProcessor;

    /**
     * Construct a new {@link TabGridPanelToolbarCoordinator}.
     *
     * @param context              The {@link Context} used to retrieve resources.
     * @param contentView          The {@link View} to which the content will
     *                             eventually be attached.
     * @param toolbarPropertyModel The {@link PropertyModel} instance representing
     *                             the toolbar.
     */
    TabGridPanelToolbarCoordinator(
            Context context, RecyclerView contentView, PropertyModel toolbarPropertyModel) {
        this(context, contentView, toolbarPropertyModel, null);
    }

    TabGridPanelToolbarCoordinator(Context context, RecyclerView contentView,
            PropertyModel toolbarPropertyModel, TabGridDialogParent dialog) {
        mToolbarView = (TabGroupUiToolbarView) LayoutInflater.from(context).inflate(
                R.layout.bottom_tab_grid_toolbar, contentView, false);
        mToolbarView.setupToolbarLayout();
        if (!FeatureUtilities.isTabGroupsAndroidContinuationEnabled()) {
            mToolbarView.hideTabGroupsContinuationWidgets();
        }
        mModelChangeProcessor = PropertyModelChangeProcessor.create(toolbarPropertyModel,
                new TabGridPanelViewBinder.ViewHolder(mToolbarView, contentView, dialog),
                TabGridPanelViewBinder::bind);
    }

    /** @return The content {@link View}. */
    View getView() {
        return mToolbarView;
    }

    /** Destroy the toolbar component. */
    @Override
    public void destroy() {
        mModelChangeProcessor.destroy();
    }
}
