// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.text.TextUtils;

import androidx.annotation.NonNull;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupColorUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabGroupCreationDialogResultAction;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabGroupCreationFinalSelections;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Objects;

/** Manager of the observers that trigger a modal dialog on new tab group creation. */
public class TabGroupCreationDialogManager implements Destroyable {
    /** The delegate for showing the dialog. */
    protected class ShowDialogDelegate {
        /**
         * Attempt to show the tab group creation dialog to the user.
         *
         * @param rootId The destination root id when creating a new tab group.
         * @param filter The current TabGroupModelFilter that this group is created on.
         */
        protected void showDialog(int rootId, TabGroupModelFilter filter) {
            ModalDialogProperties.Controller dialogController =
                    new ModalDialogProperties.Controller() {
                        @Override
                        public void onClick(PropertyModel model, int buttonType) {
                            if (buttonType == ModalDialogProperties.ButtonType.POSITIVE
                                    && !mTabGroupVisualDataDialogManager
                                            .validateCurrentGroupTitle()) {
                                mTabGroupVisualDataDialogManager.focusCurrentGroupTitle();
                                return;
                            }

                            if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                                mModalDialogManager.dismissDialog(
                                        model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                            }
                        }

                        @Override
                        public void onDismiss(
                                PropertyModel model, @DialogDismissalCause int dismissalCause) {
                            @TabGroupColorId
                            int defaultColorId =
                                    mTabGroupVisualDataDialogManager.getDefaultColorId();
                            @TabGroupColorId
                            int currentColorId =
                                    mTabGroupVisualDataDialogManager.getCurrentColorId();
                            boolean didChangeColor = currentColorId != defaultColorId;
                            filter.setTabGroupColor(rootId, currentColorId);

                            // Only save the group title input text if it has been changed from
                            // the suggested default title and if it is not empty.
                            String defaultGroupTitle =
                                    mTabGroupVisualDataDialogManager.getDefaultGroupTitle();
                            String inputGroupTitle =
                                    mTabGroupVisualDataDialogManager.getCurrentGroupTitle();
                            boolean didChangeTitle =
                                    !Objects.equals(defaultGroupTitle, inputGroupTitle);
                            if (didChangeTitle && !TextUtils.isEmpty(inputGroupTitle)) {
                                filter.setTabGroupTitle(rootId, inputGroupTitle);
                            }

                            // Refresh the GTS tab list with the newly set color and title.
                            mOnDialogAcceptedRunnable.run();
                            recordDialogSelectionHistogram(didChangeColor, didChangeTitle);

                            if (dismissalCause
                                    == DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE) {
                                TabUiMetricsHelper.recordTabGroupCreationDialogResultActionMetrics(
                                        TabGroupCreationDialogResultAction
                                                .DISMISSED_SCRIM_OR_BACKPRESS);
                            } else if (dismissalCause
                                    == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                                TabUiMetricsHelper.recordTabGroupCreationDialogResultActionMetrics(
                                        TabGroupCreationDialogResultAction.ACCEPTED);
                            } else {
                                TabUiMetricsHelper.recordTabGroupCreationDialogResultActionMetrics(
                                        TabGroupCreationDialogResultAction.DISMISSED_OTHER);
                            }

                            mTabGroupVisualDataDialogManager.hideDialog();
                        }
                    };
            mTabGroupVisualDataDialogManager.showDialog(rootId, filter, dialogController);
        }
    }

    private static final int INVALID_COLOR_ID = -1;
    private final ModalDialogManager mModalDialogManager;
    private TabModelSelector mTabModelSelector;
    private Runnable mOnDialogAcceptedRunnable;
    private TabGroupVisualDataDialogManager mTabGroupVisualDataDialogManager;
    private ShowDialogDelegate mShowDialogDelegate;
    private TabGroupModelFilterObserver mFilterObserver;

    public TabGroupCreationDialogManager(
            @NonNull Activity activity,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull Runnable onDialogAccepted) {
        mModalDialogManager = modalDialogManager;
        mTabModelSelector = tabModelSelector;
        mOnDialogAcceptedRunnable = onDialogAccepted;
        mTabGroupVisualDataDialogManager =
                new TabGroupVisualDataDialogManager(
                        activity,
                        modalDialogManager,
                        TabGroupVisualDataDialogManager.DialogType.TAB_GROUP_CREATION,
                        R.string.tab_group_creation_dialog_title);
        mShowDialogDelegate = createShowDialogDelegate();

        TabModelFilterProvider tabModelFilterProvider =
                mTabModelSelector.getTabModelFilterProvider();

        mFilterObserver =
                new TabGroupModelFilterObserver() {
                    // Handles the tab selection editor group action, longpressing a link for a
                    // context menu to create a group and the drag and dropping of single tabs.
                    @Override
                    public void didCreateNewGroup(Tab destinationTab, TabGroupModelFilter filter) {
                        // The creation dialog gets shown in certain situations when it should not
                        // be called, such as undoing group closure or unmerge when the group still
                        // technically exists. Check that the group does not already have an
                        // existing color to make sure it is truly a new group.
                        boolean isNewGroup =
                                TabGroupColorUtils.getTabGroupColor(destinationTab.getRootId())
                                        == INVALID_COLOR_ID;
                        if (isNewGroup) {
                            mShowDialogDelegate.showDialog(destinationTab.getRootId(), filter);
                        }
                    }
                };

        ((TabGroupModelFilter) tabModelFilterProvider.getTabModelFilter(false))
                .addTabGroupObserver(mFilterObserver);
        ((TabGroupModelFilter) tabModelFilterProvider.getTabModelFilter(true))
                .addTabGroupObserver(mFilterObserver);
    }

    /** Destroy any members that need clean up. */
    @Override
    public void destroy() {
        TabModelFilterProvider tabModelFilterProvider =
                mTabModelSelector.getTabModelFilterProvider();

        if (mFilterObserver != null) {
            ((TabGroupModelFilter) tabModelFilterProvider.getTabModelFilter(false))
                    .removeTabGroupObserver(mFilterObserver);
            ((TabGroupModelFilter) tabModelFilterProvider.getTabModelFilter(true))
                    .removeTabGroupObserver(mFilterObserver);
            mFilterObserver = null;
        }

        if (mTabGroupVisualDataDialogManager != null) {
            mTabGroupVisualDataDialogManager.destroy();
            mTabGroupVisualDataDialogManager = null;
        }
    }

    private ShowDialogDelegate createShowDialogDelegate() {
        return new ShowDialogDelegate();
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

    void setShowDialogDelegateForTesting(ShowDialogDelegate delegate) {
        mShowDialogDelegate = delegate;
    }

    ShowDialogDelegate getShowDialogDelegateForTesting() {
        return mShowDialogDelegate;
    }
}
