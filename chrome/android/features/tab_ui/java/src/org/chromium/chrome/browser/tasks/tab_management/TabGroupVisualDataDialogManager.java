// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.activity.ComponentDialog;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.appcompat.widget.AppCompatEditText;
import androidx.appcompat.widget.DialogTitle;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerCoordinator.ColorPickerLayoutType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** Manager of the logic to trigger a modal dialog for setting tab group visual data. */
public class TabGroupVisualDataDialogManager {
    /** Type of the dialog to be created based on expected use case. */
    @IntDef({
        TabGroupVisualDataDialogManager.DialogType.TAB_GROUP_CREATION,
        TabGroupVisualDataDialogManager.DialogType.TAB_GROUP_EDIT,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DialogType {
        int TAB_GROUP_CREATION = 0;
        int TAB_GROUP_EDIT = 1;
    }

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final @DialogType int mDialogType;
    private final @StringRes int mDialogTitleRes;
    // TODO(b/333921547): This class uses a member model rather than an instanced model in the
    // #showDialog call due to the possibility of a double show call being triggered for the
    // didCreateNewGroup observer and a fix that tackles that. Once the root cause has been fixed,
    // revert this to an instanced model within the function call for a proper lifecycle.
    private PropertyModel mModel;
    private ModalDialogManagerObserver mModalDialogManagerObserver;
    private View mCustomView;
    private TabGroupVisualDataTextInputLayout mTextInputLayout;
    private String mDefaultGroupTitle;
    private ColorPickerCoordinator mColorPickerCoordinator;
    private @TabGroupColorId int mDefaultColorId;

    /**
     * The manager responsible for handling trigger logic for tab group visual data modal dialogs.
     *
     * @param context The current context.
     * @param modalDialogManager The current modalDialogManager.
     * @param dialogType An enum describing the type of dialog to construct with regards to UI.
     * @param dialogTitleRes The resource id of the string to be used for the dialog title.
     */
    public TabGroupVisualDataDialogManager(
            @NonNull Context context,
            @NonNull ModalDialogManager modalDialogManager,
            @DialogType int dialogType,
            @StringRes int dialogTitleRes) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mDialogType = dialogType;
        mDialogTitleRes = dialogTitleRes;
    }

    /**
     * Construct and show the modal dialog for setting tab group visual data.
     *
     * @param rootId The destination root id when modifying a tab group.
     * @param filter The current TabGroupModelFilter that this group is modified on.
     * @param dialogController The dialog controller for the modal dialog's actions.
     */
    public void showDialog(
            int rootId,
            TabGroupModelFilter filter,
            ModalDialogProperties.Controller dialogController) {
        // If the model is not null, it indicates a chained double show attempt is occurring.
        // Early exit the second attempt so that we don't show another dialog and cause the
        // dialog controller and user actions to freeze when attempting to navigate out.
        if (mModel != null) {
            return;
        }

        mCustomView =
                LayoutInflater.from(mContext).inflate(R.layout.tab_group_visual_data_dialog, null);
        mTextInputLayout = mCustomView.findViewById(R.id.tab_group_title);

        DialogTitle dialogTitle = mCustomView.findViewById(R.id.visual_data_dialog_title);
        dialogTitle.setText(mDialogTitleRes);
        // Set the description text to be displayed on the dialog underneath the title.
        setDescriptionText(filter);

        // Create the default group title to be displayed in the edit text box.
        createDefaultGroupTitle(rootId, filter);
        AppCompatEditText editTextView = mCustomView.findViewById(R.id.title_input_text);
        editTextView.setText(mDefaultGroupTitle);

        List<Integer> colors = ColorPickerUtils.getTabGroupColorIdList();
        // TODO(b/330597857): Allow a dynamic incognito setting for the color picker.
        // Force a false incognito value for the color picker as this modal dialog does not
        // support incognito color themes and should just follow the system theme.
        mColorPickerCoordinator =
                new ColorPickerCoordinator(
                        mContext,
                        colors,
                        LayoutInflater.from(mContext)
                                .inflate(
                                        R.layout.tab_group_color_picker_container,
                                        /* root= */ null),
                        ColorPickerType.TAB_GROUP,
                        /* isIncognito= */ false,
                        ColorPickerLayoutType.DYNAMIC,
                        null);
        mDefaultColorId = filter.getTabGroupColorWithFallback(rootId);
        mColorPickerCoordinator.setSelectedColorItem(mDefaultColorId);

        LinearLayout linearLayout = mCustomView.findViewById(R.id.visual_data_dialog_layout);
        linearLayout.addView(mColorPickerCoordinator.getContainerView());

        // Set the modal dialog model based on the UI properties required.
        setModel(dialogController);

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
                                            WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE);
                            mModalDialogManager.removeObserver(this);
                        }
                    }
                };
        mModalDialogManager.addObserver(mModalDialogManagerObserver);
        mModalDialogManager.showDialog(mModel, ModalDialogType.APP);
    }

    /** Hide the modal dialog and destroy the necessary components. */
    public void hideDialog() {
        // Reset the model to null after each usage.
        mModel = null;
        if (mModalDialogManagerObserver != null) {
            mModalDialogManager.removeObserver(mModalDialogManagerObserver);
        }
    }

    /** Get the current group title that is displayed in the modal dialog. */
    public String getCurrentGroupTitle() {
        return mTextInputLayout.getTrimmedText();
    }

    /** Validate that the current group title is non empty. */
    public boolean validateCurrentGroupTitle() {
        return mTextInputLayout.validate();
    }

    /** Focus the edit box for the group title entry in the modal dialog. */
    public void focusCurrentGroupTitle() {
        mTextInputLayout.requestFocus();
    }

    /** Get the default group title displayed on show dialog. */
    public String getDefaultGroupTitle() {
        return mDefaultGroupTitle;
    }

    /** Get the default group color displayed on show dialog. */
    public @TabGroupColorId int getDefaultColorId() {
        return mDefaultColorId;
    }

    public @TabGroupColorId int getCurrentColorId() {
        return mColorPickerCoordinator.getSelectedColorSupplier().get();
    }

    private void createDefaultGroupTitle(int rootId, TabGroupModelFilter filter) {
        int tabCount = filter.getRelatedTabCountForRootId(rootId);
        String defaultGroupTitle =
                mContext.getResources()
                        .getQuantityString(
                                R.plurals.bottom_tab_grid_title_placeholder, tabCount, tabCount);

        if (mDialogType == DialogType.TAB_GROUP_CREATION) {
            mDefaultGroupTitle = defaultGroupTitle;
        } else if (mDialogType == DialogType.TAB_GROUP_EDIT) {
            mDefaultGroupTitle = filter.getTabGroupTitle(rootId);

            if (mDefaultGroupTitle == null) {
                mDefaultGroupTitle = defaultGroupTitle;
            }
        }
    }

    private void setDescriptionText(TabGroupModelFilter filter) {
        if (mDialogType == DialogType.TAB_GROUP_CREATION) {
            TabModel tabModel = filter.getTabModel();
            Profile profile = tabModel.getProfile();
            TextView descriptionView =
                    mCustomView.findViewById(R.id.visual_data_dialog_description);
            Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
            // Only set text if the current model is not incognito, and both the TabGroupSync and
            // TabGroupPane flags are enabled.
            if (!tabModel.isIncognitoBranded()
                    && TabGroupSyncFeatures.isTabGroupSyncEnabled(profile)
                    && ChromeFeatureList.sTabGroupPaneAndroid.isEnabled()
                    && tracker.shouldTriggerHelpUI(
                            FeatureConstants.TAB_GROUP_CREATION_DIALOG_SYNC_TEXT_FEATURE)) {
                descriptionView.setVisibility(View.VISIBLE);
                SyncService syncService = SyncServiceFactory.getForProfile(profile);
                boolean syncingTabGroups =
                        syncService.getActiveDataTypes().contains(DataType.SAVED_TAB_GROUP);

                // Set description text based on if sync is enabled.
                final @StringRes int descriptionId =
                        syncingTabGroups
                                ? R.string.tab_group_creation_dialog_description_text_sync_on
                                : R.string.tab_group_creation_dialog_description_text_sync_off;
                String descriptionText = mContext.getResources().getString(descriptionId);
                descriptionView.setText(descriptionText);

                tracker.notifyEvent(EventConstants.TAB_GROUP_CREATION_DIALOG_SHOWN);
            } else {
                descriptionView.setVisibility(View.GONE);
            }
        }
    }

    private void setModel(ModalDialogProperties.Controller dialogController) {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mCustomView);

        if (mDialogType == DialogType.TAB_GROUP_CREATION) {
            builder.with(
                            ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                            mContext.getResources()
                                    .getString(R.string.tab_group_creation_positive_button_text))
                    .with(
                            ModalDialogProperties.BUTTON_STYLES,
                            ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NO_NEGATIVE);
        } else if (mDialogType == DialogType.TAB_GROUP_EDIT) {
            builder.with(
                            ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                            mContext.getResources()
                                    .getString(R.string.tab_group_rename_positive_button_text))
                    .with(
                            ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                            mContext.getResources()
                                    .getString(R.string.tab_group_rename_negative_button_text))
                    .with(
                            ModalDialogProperties.BUTTON_STYLES,
                            ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE);
        }

        mModel = builder.build();
    }
}
