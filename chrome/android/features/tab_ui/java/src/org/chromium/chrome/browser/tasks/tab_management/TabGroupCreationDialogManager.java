// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabGroupCreationDialogResultAction;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabGroupCreationFinalSelections;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Objects;

/** Manager of the observers that trigger a modal dialog on new tab group creation. */
@NullMarked
public class TabGroupCreationDialogManager {
    /** Represents a factory for creating an instance of {@link TabGroupCreationDialogManager}. */
    @FunctionalInterface
    public interface TabGroupCreationDialogManagerFactory {
        TabGroupCreationDialogManager create(
                Context context,
                ModalDialogManager modalDialogManager,
                @Nullable Runnable onTabGroupCreation);
    }

    private class TabGroupCreationDialogController implements Controller {
        private final Token mTabGroupId;
        private final TabGroupModelFilter mTabGroupModelFilter;

        private TabGroupCreationDialogController(
                @Nullable Token tabGroupId, TabGroupModelFilter tabGroupModelFilter) {
            assert tabGroupId != null;
            assert tabGroupModelFilter.tabGroupExists(tabGroupId);

            mTabGroupId = tabGroupId;
            mTabGroupModelFilter = tabGroupModelFilter;
        }

        @Override
        public void onClick(PropertyModel model, int buttonType) {
            if (buttonType == ModalDialogProperties.ButtonType.POSITIVE
                    && !mTabGroupVisualDataDialogManager.validateCurrentGroupTitle()) {
                mTabGroupVisualDataDialogManager.focusCurrentGroupTitle();
                return;
            }

            if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                mModalDialogManager.dismissDialog(
                        model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
            }
        }

        @Override
        public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
            boolean stillExists = mTabGroupModelFilter.tabGroupExists(mTabGroupId);

            final @TabGroupColorId int defaultColorId =
                    mTabGroupVisualDataDialogManager.getDefaultColorId();
            final @TabGroupColorId int currentColorId =
                    mTabGroupVisualDataDialogManager.getCurrentColorId();
            boolean didChangeColor = currentColorId != defaultColorId;
            if (stillExists) {
                mTabGroupModelFilter.setTabGroupColor(mTabGroupId, currentColorId);
            }

            // Only save the group title input text if it has been changed from the suggested
            // initial title and if it is not empty.
            String initialGroupTitle = mTabGroupVisualDataDialogManager.getInitialGroupTitle();
            String inputGroupTitle = mTabGroupVisualDataDialogManager.getCurrentGroupTitle();
            boolean didChangeTitle = !Objects.equals(initialGroupTitle, inputGroupTitle);
            if (didChangeTitle && !TextUtils.isEmpty(inputGroupTitle) && stillExists) {
                mTabGroupModelFilter.setTabGroupTitle(mTabGroupId, inputGroupTitle);
            }

            recordDialogSelectionHistogram(didChangeColor, didChangeTitle);

            if (dismissalCause == DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE) {
                TabUiMetricsHelper.recordTabGroupCreationDialogResultActionMetrics(
                        TabGroupCreationDialogResultAction.DISMISSED_SCRIM_OR_BACKPRESS);
            } else if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                TabUiMetricsHelper.recordTabGroupCreationDialogResultActionMetrics(
                        TabGroupCreationDialogResultAction.ACCEPTED);
            } else {
                TabUiMetricsHelper.recordTabGroupCreationDialogResultActionMetrics(
                        TabGroupCreationDialogResultAction.DISMISSED_OTHER);
            }

            mTabGroupVisualDataDialogManager.onHideDialog();
            if (mOnTabGroupCreation != null) {
                mOnTabGroupCreation.run();
            }
        }
    }

    private final ModalDialogManager mModalDialogManager;
    private final @Nullable Runnable mOnTabGroupCreation;
    private TabGroupVisualDataDialogManager mTabGroupVisualDataDialogManager;
    private @Nullable Controller mTabGroupCreationDialogController;

    public TabGroupCreationDialogManager(
            Context context,
            ModalDialogManager modalDialogManager,
            @Nullable Runnable onTabGroupCreation) {
        mModalDialogManager = modalDialogManager;
        mOnTabGroupCreation = onTabGroupCreation;
        mTabGroupVisualDataDialogManager =
                new TabGroupVisualDataDialogManager(
                        context,
                        modalDialogManager,
                        TabGroupVisualDataDialogManager.DialogType.TAB_GROUP_CREATION,
                        R.string.tab_group_creation_dialog_title);
    }

    /**
     * Attempt to show the tab group creation dialog to the user. The current use case for this
     * dialog means that it is shown after the group has already been merged.
     *
     * @param tabGroupId The destination tab group id of the new tab group that has been created.
     * @param filter The current TabGroupModelFilter that this group is created on.
     */
    public void showDialog(@Nullable Token tabGroupId, TabGroupModelFilter filter) {
        mTabGroupCreationDialogController =
                new TabGroupCreationDialogController(tabGroupId, filter);
        mTabGroupVisualDataDialogManager.showDialog(
                tabGroupId, filter, mTabGroupCreationDialogController);
    }

    private void recordDialogSelectionHistogram(boolean didChangeColor, boolean didChangeTitle) {
        if (didChangeColor && didChangeTitle) {
            TabUiMetricsHelper.recordTabGroupCreationFinalSelectionsHistogram(
                    TabGroupCreationFinalSelections.CHANGED_COLOR_AND_TITLE);
        } else {
            if (didChangeColor) {
                TabUiMetricsHelper.recordTabGroupCreationFinalSelectionsHistogram(
                        TabGroupCreationFinalSelections.CHANGED_COLOR);
            } else if (didChangeTitle) {
                TabUiMetricsHelper.recordTabGroupCreationFinalSelectionsHistogram(
                        TabGroupCreationFinalSelections.CHANGED_TITLE);
            } else {
                TabUiMetricsHelper.recordTabGroupCreationFinalSelectionsHistogram(
                        TabGroupCreationFinalSelections.DEFAULT_COLOR_AND_TITLE);
            }
        }
    }

    void setDialogManagerForTesting(TabGroupVisualDataDialogManager manager) {
        mTabGroupVisualDataDialogManager = manager;
    }

    @Nullable Controller getDialogControllerForTesting() {
        return mTabGroupCreationDialogController;
    }
}
