// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;
import android.widget.LinearLayout;

import androidx.activity.ComponentDialog;
import androidx.annotation.NonNull;
import androidx.appcompat.widget.AppCompatEditText;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupColorUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator.ColorPickerLayoutType;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabGroupCreationDialogResultAction;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabGroupCreationFinalSelections;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
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
            // If the model is not null, it indicates a chained double show attempt is occurring.
            // Early exit the second attempt so that we don't show another dialog and cause the
            // dialog controller and user actions to freeze when attempting to navigate out.
            if (mModel != null) {
                TabUiMetricsHelper.recordTabGroupCreationDialogResultActionMetrics(
                        TabGroupCreationDialogResultAction.DISMISSED_OTHER);
                return;
            }

            int tabCount = filter.getRelatedTabCountForRootId(rootId);
            String defaultGroupTitle =
                    mActivity
                            .getResources()
                            .getQuantityString(
                                    R.plurals.bottom_tab_grid_title_placeholder,
                                    tabCount,
                                    tabCount);

            View customView =
                    LayoutInflater.from(mActivity)
                            .inflate(R.layout.tab_group_creation_dialog, null);
            AppCompatEditText editTextView = customView.findViewById(R.id.title_input_text);
            editTextView.setText(defaultGroupTitle);

            List<Integer> colors = ColorPickerUtils.getTabGroupColorIdList();
            // TODO(b/330597857): Allow a dynamic incognito setting for the color picker.
            // Force a false incognito value for the color picker as this modal dialog does not
            // support incognito color themes and should just follow the system theme.
            ColorPickerCoordinator colorPickerCoordinator =
                    new ColorPickerCoordinator(
                            mActivity,
                            colors,
                            R.layout.tab_group_color_picker_container,
                            ColorPickerType.TAB_GROUP,
                            /* isIncognito= */ false,
                            ColorPickerLayoutType.DYNAMIC,
                            null);
            final @TabGroupColorId int defaultColorId = filter.getTabGroupColor(rootId);
            colorPickerCoordinator.setSelectedColorItem(defaultColorId);

            LinearLayout linearLayout = customView.findViewById(R.id.creation_dialog_layout);
            linearLayout.addView(colorPickerCoordinator.getContainerView());

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

                            if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                                mModalDialogManager.dismissDialog(
                                        model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                            }
                        }

                        @Override
                        public void onDismiss(PropertyModel model, int dismissalCause) {
                            if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                    || dismissalCause
                                            == DialogDismissalCause
                                                    .NAVIGATE_BACK_OR_TOUCH_OUTSIDE) {
                                final @TabGroupColorId int currentColorId =
                                        colorPickerCoordinator.getSelectedColorSupplier().get();
                                boolean didChangeColor = currentColorId != defaultColorId;
                                filter.setTabGroupColor(rootId, currentColorId);

                                // Only save the group title input text if it has been changed from
                                // the suggested default title and if it is not empty.
                                String inputGroupTitle = groupTitle.getTrimmedText();
                                boolean didChangeTitle =
                                        !Objects.equals(defaultGroupTitle, inputGroupTitle);
                                if (didChangeTitle && !TextUtils.isEmpty(inputGroupTitle)) {
                                    filter.setTabGroupTitle(rootId, groupTitle.getTrimmedText());
                                }

                                // Refresh the GTS tab list with the newly set color and title.
                                mOnDialogAcceptedRunnable.run();
                                recordDialogSelectionHistogram(didChangeColor, didChangeTitle);

                                if (dismissalCause
                                        == DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE) {
                                    TabUiMetricsHelper
                                            .recordTabGroupCreationDialogResultActionMetrics(
                                                    TabGroupCreationDialogResultAction
                                                            .DISMISSED_SCRIM_OR_BACKPRESS);
                                } else {
                                    TabUiMetricsHelper
                                            .recordTabGroupCreationDialogResultActionMetrics(
                                                    TabGroupCreationDialogResultAction.ACCEPTED);
                                }
                            } else {
                                TabUiMetricsHelper.recordTabGroupCreationDialogResultActionMetrics(
                                        TabGroupCreationDialogResultAction.DISMISSED_OTHER);
                            }

                            mModalDialogManager.removeObserver(mModalDialogManagerObserver);
                            // Reset the model to null after each usage.
                            mModel = null;
                        }
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
                            .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                            .with(
                                    ModalDialogProperties.BUTTON_STYLES,
                                    ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NO_NEGATIVE)
                            .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                            .build();

            mModalDialogManagerObserver =
                    new ModalDialogManagerObserver() {
                        @Override
                        public void onDialogCreated(PropertyModel model, ComponentDialog dialog) {
                            // Ensure that this dialog's model is the one that's being acted upon.
                            if (model == mModel) {
                                // Focus the edit text and display the keyboard on dialog showing.
                                editTextView.requestFocus();
                                dialog.getWindow()
                                        .setSoftInputMode(
                                                WindowManager.LayoutParams
                                                        .SOFT_INPUT_STATE_VISIBLE);
                            }
                        }
                    };
            mModalDialogManager.addObserver(mModalDialogManagerObserver);
            mModalDialogManager.showDialog(mModel, ModalDialogType.APP);
        }
    }

    private static final int INVALID_COLOR_ID = -1;

    private final Activity mActivity;
    private final ModalDialogManager mModalDialogManager;
    private TabModelSelector mTabModelSelector;
    private TabGroupModelFilterObserver mFilterObserver;
    // TODO(b/333921547): This class uses a member model rather than an instanced model in the
    // #showDialog call due to the possibility of a double show call being triggered for the
    // didCreateNewGroup observer and a fix that tackles that. Once the root cause has been fixed,
    // revert this to an instanced model within the function call for a proper lifecycle.
    private PropertyModel mModel;
    private ShowDialogDelegate mShowDialogDelegate;
    private Runnable mOnDialogAcceptedRunnable;
    private ModalDialogManagerObserver mModalDialogManagerObserver;

    public TabGroupCreationDialogManager(
            @NonNull Activity activity,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull Runnable onDialogAccepted) {
        mActivity = activity;
        mModalDialogManager = modalDialogManager;
        mTabModelSelector = tabModelSelector;
        mShowDialogDelegate = createShowDialogDelegate();
        mOnDialogAcceptedRunnable = onDialogAccepted;

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

        if (mModalDialogManagerObserver != null) {
            mModalDialogManager.removeObserver(mModalDialogManagerObserver);
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
