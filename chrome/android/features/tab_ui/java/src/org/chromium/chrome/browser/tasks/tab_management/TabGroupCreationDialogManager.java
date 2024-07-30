// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabGroupCreationDialogResultAction;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabGroupCreationFinalSelections;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Objects;

/** Manager of the observers that trigger a modal dialog on new tab group creation. */
public class TabGroupCreationDialogManager {
    private class TabGroupCreationDialogController implements ModalDialogProperties.Controller {
        private int mRootId;
        private TabGroupModelFilter mTabGroupModelFilter;

        private TabGroupCreationDialogController(
                int rootId, TabGroupModelFilter tabGroupModelFilter) {
            mRootId = rootId;
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
            final @TabGroupColorId int defaultColorId =
                    mTabGroupVisualDataDialogManager.getDefaultColorId();
            final @TabGroupColorId int currentColorId =
                    mTabGroupVisualDataDialogManager.getCurrentColorId();
            boolean didChangeColor = currentColorId != defaultColorId;
            mTabGroupModelFilter.setTabGroupColor(mRootId, currentColorId);

            // Only save the group title input text if it has been changed from the suggested
            // default title and if it is not empty.
            String defaultGroupTitle = mTabGroupVisualDataDialogManager.getDefaultGroupTitle();
            String inputGroupTitle = mTabGroupVisualDataDialogManager.getCurrentGroupTitle();
            boolean didChangeTitle = !Objects.equals(defaultGroupTitle, inputGroupTitle);
            if (didChangeTitle && !TextUtils.isEmpty(inputGroupTitle)) {
                mTabGroupModelFilter.setTabGroupTitle(mRootId, inputGroupTitle);
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

            TrackerFactory.getTrackerForProfile(mTabGroupModelFilter.getTabModel().getProfile())
                    .dismissed(FeatureConstants.TAB_GROUP_CREATION_DIALOG_SYNC_TEXT_FEATURE);

            mTabGroupVisualDataDialogManager.hideDialog();
            if (mOnTabGroupCreation != null) {
                mOnTabGroupCreation.run();
            }
        }
    }

    @NonNull private final ModalDialogManager mModalDialogManager;
    @Nullable private final Runnable mOnTabGroupCreation;
    private TabGroupVisualDataDialogManager mTabGroupVisualDataDialogManager;
    private ModalDialogProperties.Controller mTabGroupCreationDialogController;

    public TabGroupCreationDialogManager(
            @NonNull Context context,
            @NonNull ModalDialogManager modalDialogManager,
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
     * @param rootId The destination root id of the new tab group that has been created.
     * @param filter The current TabGroupModelFilter that this group is created on.
     */
    public void showDialog(int rootId, TabGroupModelFilter filter) {
        mTabGroupCreationDialogController = new TabGroupCreationDialogController(rootId, filter);
        mTabGroupVisualDataDialogManager.showDialog(
                rootId, filter, mTabGroupCreationDialogController);
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

    ModalDialogProperties.Controller getDialogControllerForTesting() {
        return mTabGroupCreationDialogController;
    }

    /**
     * Returns whether the group creation dialog will be skipped based on current flags.
     *
     * @param shouldShow Whether the creation dialog should show if TabGroupCreationDialogAndroid is
     *     enabled. Currently it should only show for drag and drop merge and bulk selection editor
     *     merge. It should not show for context menu group creations.
     */
    public static boolean shouldSkipGroupCreationDialog(boolean shouldShow) {
        if (ChromeFeatureList.sTabGroupCreationDialogAndroid.isEnabled()) {
            return !shouldShow;
        } else {
            return TabGroupModelFilter.SKIP_TAB_GROUP_CREATION_DIALOG.getValue();
        }
    }

    /**
     * Returns whether the group creation dialog should be shown based on the setting switch for
     * auto showing under tab settings. If it is not enabled, return true since that is the default
     * case for all callsites.
     */
    public static boolean shouldShowGroupCreationDialogViaSettingsSwitch() {
        return TabGroupModelFilter.shouldShowGroupCreationDialogViaSettingsSwitch();
    }
}
