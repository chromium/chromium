// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.appcompat.widget.AppCompatEditText;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator.ColorPickerLayoutType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Manager of the observers that trigger a modal dialog on new tab group creation. */
public class TabGroupCreationDialogManager implements Destroyable {
    /** The delegate for showing the dialog. */
    protected class ShowDialogDelegate {
        /**
         * Attempt to show the tab group creation dialog to the user.
         *
         * @param tabCount The total tab count when creating the tab group.
         * @param isIncognito Whether the current tab model is incognito.
         */
        protected void showDialog(int tabCount, boolean isIncognito) {
            View customView =
                    LayoutInflater.from(mActivity)
                            .inflate(R.layout.tab_group_creation_dialog, null);
            ((AppCompatEditText) customView.findViewById(R.id.title_input_text))
                    .setText(
                            mActivity
                                    .getResources()
                                    .getQuantityString(
                                            R.plurals.bottom_tab_grid_title_placeholder,
                                            tabCount,
                                            tabCount));

            List<Integer> colors = ColorPickerUtils.getTabGroupColorIdList();
            ColorPickerCoordinator colorPickerCoordinator =
                    new ColorPickerCoordinator(
                            mActivity,
                            colors,
                            R.layout.tab_group_color_picker_container,
                            ColorPickerType.TAB_GROUP,
                            isIncognito,
                            ColorPickerLayoutType.DYNAMIC,
                            null);
            colorPickerCoordinator.setSelectedColorItem(colors.get(1));

            TabGroupCreationTextInputLayout groupTitle =
                    customView.findViewById(R.id.tab_group_title);
            ModalDialogProperties.Controller dialogController =
                    new ModalDialogProperties.Controller() {
                        @Override
                        public void onClick(PropertyModel model, int buttonType) {
                            if (buttonType == ModalDialogProperties.ButtonType.POSITIVE
                                    && !groupTitle.validate()) {
                                groupTitle.requestFocus();
                                return;
                            }

                            final @DialogDismissalCause int cause;
                            if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                                // TODO(crbug.com/1517346): Save title and color and delay undo
                                // snackbar for drag and drop and selection editor.
                                cause = DialogDismissalCause.POSITIVE_BUTTON_CLICKED;
                            } else {
                                // TODO(crbug.com/1517346): Enact the snackbar undo function if
                                // applicable.
                                cause = DialogDismissalCause.NEGATIVE_BUTTON_CLICKED;
                            }

                            mModalDialogManager.dismissDialog(mModel, cause);
                        }

                        // TODO(crbug.com/1517346): On unexpected dismissal, save both the
                        // default and user edited title and color.
                        @Override
                        public void onDismiss(PropertyModel model, int dismissalCause) {}
                    };

            mModel =
                    new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                            .with(ModalDialogProperties.CONTROLLER, dialogController)
                            .with(
                                    ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                    mActivity
                                            .getResources()
                                            .getString(
                                                    R.string
                                                            .tab_group_creation_positive_button_text))
                            .with(
                                    ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                    mActivity.getResources().getString(R.string.cancel))
                            .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                            .with(
                                    ModalDialogProperties.BUTTON_STYLES,
                                    ModalDialogProperties.ButtonStyles
                                            .PRIMARY_FILLED_NEGATIVE_OUTLINE)
                            .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                            .build();

            mModalDialogManager.showDialog(mModel, ModalDialogType.APP);
        }
    }

    private final Activity mActivity;
    private final ModalDialogManager mModalDialogManager;
    private TabModelSelector mTabModelSelector;
    private TabGroupModelFilterObserver mFilterObserver;
    private PropertyModel mModel;
    private ShowDialogDelegate mShowDialogDelegate;

    public TabGroupCreationDialogManager(
            @NonNull Activity activity,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull TabModelSelector tabModelSelector) {
        mActivity = activity;
        mModalDialogManager = modalDialogManager;
        mTabModelSelector = tabModelSelector;
        mShowDialogDelegate = createShowDialogDelegate();

        TabModelFilterProvider tabModelFilterProvider =
                mTabModelSelector.getTabModelFilterProvider();

        mFilterObserver =
                new TabGroupModelFilterObserver() {
                    // Handles the tab selection editor group action, longpressing a link for a
                    // context menu to create a group and the drag and dropping of single tabs.
                    @Override
                    public void didCreateNewGroup(Tab destinationTab, TabGroupModelFilter filter) {
                        // TODO(crbug.com/1517346): Consider removing the cancel button for
                        // longpress add as the undo flow does not exist there.
                        mShowDialogDelegate.showDialog(
                                filter.getRelatedTabCountForRootId(destinationTab.getRootId()),
                                filter.isIncognito());
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
    }

    private ShowDialogDelegate createShowDialogDelegate() {
        return new ShowDialogDelegate();
    }

    void setShowDialogDelegateForTesting(ShowDialogDelegate delegate) {
        mShowDialogDelegate = delegate;
    }

    ShowDialogDelegate getShowDialogDelegateForTesting() {
        return mShowDialogDelegate;
    }
}
