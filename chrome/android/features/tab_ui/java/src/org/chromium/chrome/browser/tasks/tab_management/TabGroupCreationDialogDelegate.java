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
import org.chromium.chrome.browser.tasks.tab_groups.EmptyTabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Delegate that manages the observer for the modal dialog on new tab group creation. */
public class TabGroupCreationDialogDelegate {
    private final Activity mActivity;
    private final ModalDialogManager mModalDialogManager;
    private ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private EmptyTabGroupModelFilterObserver mRegularObserver;
    private EmptyTabGroupModelFilterObserver mIncognitoObserver;
    private PropertyModel mModel;

    public TabGroupCreationDialogDelegate(
            @NonNull Activity activity,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull ObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        mActivity = activity;
        mModalDialogManager = modalDialogManager;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
    }

    public void destroy() {
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

        if (mTabModelSelectorSupplier != null) {
            mTabModelSelectorSupplier = null;
        }
    }

    // TODO(crbug.com/1517346): Make this private and initialize it only when TabModelSelector's tab
    // state has been initialized.
    /** Add an EmptyTabGroupModelFilterObserver to notify when a new tab group is being created. */
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

    private EmptyTabGroupModelFilterObserver attachObserver(TabGroupModelFilter filter) {
        EmptyTabGroupModelFilterObserver observer =
                new EmptyTabGroupModelFilterObserver() {
                    // Handles the tab selection editor group action and longpressing a link for a
                    // context menu to create a group.
                    @Override
                    public void didCreateNewGroup(int newRootId) {
                        // TODO(crbug.com/1517346): Consider removing the cancel button for
                        // longpress add as the undo flow does not exist there.
                        showDialog(filter.getRelatedTabCountForRootId(newRootId));
                    }

                    // Handles the drag and dropping of two single tabs to create a group.
                    @Override
                    public void willMergeTabToGroup(Tab movedTab, int newRootId) {
                        int newRootIdTabCount = filter.getRelatedTabCountForRootId(newRootId);
                        // If the items being merged are groups, do not show the modal dialog and
                        // early exit.
                        if (filter.hasOtherRelatedTabs(movedTab) || newRootIdTabCount > 1) {
                            return;
                        }
                        // Pass in the tab count of the two tab items to be merged.
                        showDialog(newRootIdTabCount + 1);
                    }
                };
        filter.addTabGroupObserver(observer);
        return observer;
    }

    protected void showDialog(int tabCount) {
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
