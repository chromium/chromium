// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks.ui;

import android.content.res.Resources;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab_bottom_sheet.PeekViewManager;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetManager;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetPeekProperties;
import org.chromium.components.browser_ui.styles.R;
import org.chromium.ui.modelutil.PropertyModel;

/** Coordinator to manage the Contextual Tasks peek view in the tab bottom sheet. */
@NullMarked
public class ContextualTasksControlCoordinator implements PeekViewManager {
    private final TabBottomSheetManager mTabBottomSheetManager;
    private final PropertyModel mModel;

    /**
     * Constructs the coordinator.
     *
     * @param tabBottomSheetManager The {@link TabBottomSheetManager} to use.
     */
    public ContextualTasksControlCoordinator(TabBottomSheetManager tabBottomSheetManager) {
        mTabBottomSheetManager = tabBottomSheetManager;

        mModel =
                new PropertyModel.Builder(TabBottomSheetPeekProperties.ALL_KEYS)
                        .with(TabBottomSheetPeekProperties.TITLE_TEXT, "")
                        .with(
                                TabBottomSheetPeekProperties.TITLE_TEXT_APPEARANCE_ID,
                                R.style.TextAppearance_TextMediumThick_Primary)
                        .with(TabBottomSheetPeekProperties.DESCRIPTION_TEXT_ID, Resources.ID_NULL)
                        .with(TabBottomSheetPeekProperties.DESCRIPTION_VISIBILITY, View.GONE)
                        .with(TabBottomSheetPeekProperties.ACTION_BUTTON_TEXT_ID, Resources.ID_NULL)
                        .with(TabBottomSheetPeekProperties.ACTION_BUTTON_VISIBILITY, View.GONE)
                        .with(
                                TabBottomSheetPeekProperties.PEEK_ICON_ID,
                                R.drawable.ic_logo_googleg_24dp)
                        .with(
                                TabBottomSheetPeekProperties.ON_ACTION_BUTTON_CLICKED,
                                this::onActionClicked)
                        .with(TabBottomSheetPeekProperties.ON_CLOSE_CLICKED, this::onCloseClicked)
                        .with(
                                TabBottomSheetPeekProperties.ON_PEEK_VIEW_CLICKED,
                                this::onPeekViewClicked)
                        .build();

        new ContextualTasksControlMediator(mModel);
    }

    private void onActionClicked() {
        mTabBottomSheetManager.setSheetExpanded(true);
    }

    private void onCloseClicked() {
        mTabBottomSheetManager.tryToCloseBottomSheet(/* animate= */ true);
    }

    private void onPeekViewClicked() {
        mTabBottomSheetManager.setSheetExpanded(true);
    }

    @Override
    public PropertyModel getModel() {
        return mModel;
    }

    @Override
    public void destroy() {}
}
