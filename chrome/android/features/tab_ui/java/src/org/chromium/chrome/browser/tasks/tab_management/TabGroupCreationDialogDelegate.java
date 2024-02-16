// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.appcompat.widget.AppCompatEditText;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Delegate that manages the observer for the modal dialog on new tab group creation. */
public class TabGroupCreationDialogDelegate implements TabGroupCreationDialog {
    private final Activity mActivity;
    private final ModalDialogManager mModalDialogManager;
    private ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private TabGroupModelFilterObserver mRegularObserver;
    private TabGroupModelFilterObserver mIncognitoObserver;
    private PropertyModel mModel;

    public TabGroupCreationDialogDelegate(
            @NonNull Activity activity,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull ObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        mActivity = activity;
        mModalDialogManager = modalDialogManager;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
    }

    @Override
    public void destroy() {
        detachObservers();

        if (mTabModelSelectorSupplier != null) {
            mTabModelSelectorSupplier = null;
        }
    }

    // TODO(crbug.com/1517346): Make this private and initialize it only when TabModelSelector's tab
    // state has been initialized.
    /** Add an TabGroupModelFilterObserver to notify when a new tab group is being created. */
    @Override
    public void addObservers() {
        TabModelFilterProvider tabModelFilterProvider =
                mTabModelSelectorSupplier.get().getTabModelFilterProvider();

        mRegularObserver =
                attachObserver(
                        (TabGroupModelFilter) tabModelFilterProvider.getTabModelFilter(false));
        mIncognitoObserver =
                attachObserver(
                        (TabGroupModelFilter) tabModelFilterProvider.getTabModelFilter(true));
    }

    private TabGroupModelFilterObserver attachObserver(TabGroupModelFilter filter) {
        TabGroupModelFilterObserver observer =
                new TabGroupModelFilterObserver() {
                    // Handles the tab selection editor group action and longpressing a link for a
                    // context menu to create a group.
                    @Override
                    public void didCreateNewGroup(int newRootId) {
                        // TODO(crbug.com/1517346): Consider removing the cancel button for
                        // longpress add as the undo flow does not exist there.
                        showDialog(
                                filter.getRelatedTabCountForRootId(newRootId),
                                filter.isIncognito());
                    }

                    // Handles the drag and dropping of two single tabs to create a group.
                    @Override
                    public void willMergeTabToGroup(Tab movedTab, int newRootId) {
                        // Moving tab is already in a group.
                        if (filter.isTabInTabGroup(movedTab)) return;

                        // Target root ID already has a group.
                        Tab groupTab = filter.getGroupLastShownTab(newRootId);
                        assert groupTab != null : "Merging tab to empty group.";
                        if (filter.isTabInTabGroup(groupTab)) return;

                        // Pass in the tab count of the two tab items to be merged.
                        int newRootIdTabCount = filter.getRelatedTabCountForRootId(newRootId);
                        showDialog(newRootIdTabCount + 1, filter.isIncognito());
                    }
                };
        filter.addTabGroupObserver(observer);
        return observer;
    }

    /** Remove observers monitoring tab group creation that display a custom dialog. */
    @Override
    public void removeObservers() {
        detachObservers();
    }

    private void detachObservers() {
        TabModelFilterProvider tabModelFilterProvider =
                mTabModelSelectorSupplier.get().getTabModelFilterProvider();

        if (mRegularObserver != null) {
            ((TabGroupModelFilter) tabModelFilterProvider.getTabModelFilter(false))
                    .removeTabGroupObserver(mRegularObserver);
            mRegularObserver = null;
        }

        if (mIncognitoObserver != null) {
            ((TabGroupModelFilter) tabModelFilterProvider.getTabModelFilter(true))
                    .removeTabGroupObserver(mIncognitoObserver);
            mIncognitoObserver = null;
        }
    }

    protected void showDialog(int tabCount, boolean isIncognito) {
        View customView =
                LayoutInflater.from(mActivity).inflate(R.layout.tab_group_creation_dialog, null);
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
                        isIncognito);
        colorPickerCoordinator.setSelectedColorItem(colors.get(1));

        TabGroupCreationTextInputLayout groupTitle = customView.findViewById(R.id.tab_group_title);
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
                            // TODO(crbug.com/1517346): Save title and color and delay undo snackbar
                            // for drag and drop and selection editor.
                            cause = DialogDismissalCause.POSITIVE_BUTTON_CLICKED;
                        } else {
                            // TODO(crbug.com/1517346): Enact the snackbar undo function if
                            // applicable.
                            cause = DialogDismissalCause.NEGATIVE_BUTTON_CLICKED;
                        }

                        mModalDialogManager.dismissDialog(mModel, cause);
                    }

                    // TODO(crbug.com/1517346): On unexpected dismissal, save either the default or
                    // user edited title and color.
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
                                                R.string.tab_group_creation_positive_button_text))
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mActivity.getResources().getString(R.string.cancel))
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .build();

        mModalDialogManager.showDialog(mModel, ModalDialogType.APP);
    }
}
