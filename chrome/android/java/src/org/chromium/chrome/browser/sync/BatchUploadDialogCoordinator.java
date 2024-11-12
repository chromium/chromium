// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.IdRes;
import androidx.annotation.MainThread;
import androidx.annotation.PluralsRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.MaterialSwitchWithTitleAndSummary;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.LocalDataDescription;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;

/**
 * A confirmation dialog for settings batch upload. It will allow the user to select which data
 * types to upload. Data type switches will be displayed only if the user has local data for that
 * particular type.
 */
public final class BatchUploadDialogCoordinator {
    public interface Listener {
        /** Called when the user clicks the button. */
        void onSaveInAccountDialogButtonClicked(Set<Integer> types, int itemsCount);
    }

    private final ModalDialogManager mDialogManager;
    private final PropertyModel mModel;
    private final Context mContext;
    private final MaterialSwitchWithTitleAndSummary mBookmarkSwitch;
    private final MaterialSwitchWithTitleAndSummary mPasswordsSwitch;
    private final MaterialSwitchWithTitleAndSummary mReadingListSwitch;

    @MainThread
    public static void show(
            Context context,
            HashMap<Integer, LocalDataDescription> localDataDescriptionsMap,
            ModalDialogManager dialogManager,
            Listener listener) {
        ThreadUtils.assertOnUiThread();
        new BatchUploadDialogCoordinator(
                context, localDataDescriptionsMap, dialogManager, listener);
    }

    @VisibleForTesting
    @MainThread
    BatchUploadDialogCoordinator(
            Context context,
            HashMap<Integer, LocalDataDescription> localDataDescriptionsMap,
            ModalDialogManager dialogManager,
            Listener listener) {
        mContext = context;
        mDialogManager = dialogManager;

        final View view = inflateView(context);

        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(
                                ModalDialogProperties.TITLE,
                                mContext.getString(
                                        R.string.account_settings_bulk_upload_dialog_title))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mContext.getString(
                                        R.string.account_settings_bulk_upload_dialog_save_button))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mContext.getString(
                                        R.string.account_settings_bulk_upload_dialog_cancel_button))
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(ModalDialogProperties.CUSTOM_VIEW, view)
                        .with(
                                ModalDialogProperties.CONTROLLER,
                                createController(localDataDescriptionsMap, listener))
                        .build();
        mDialogManager.showDialog(mModel, ModalDialogType.APP);

        mBookmarkSwitch =
                updateDataTypeSwitch(
                        context,
                        view,
                        DataType.BOOKMARKS,
                        R.id.account_settings_bulk_upload_dialog_bookmarks,
                        R.plurals.account_settings_bulk_upload_dialog_bookmarks,
                        localDataDescriptionsMap);

        mPasswordsSwitch =
                updateDataTypeSwitch(
                        context,
                        view,
                        DataType.PASSWORDS,
                        R.id.account_settings_bulk_upload_dialog_passwords,
                        R.plurals.account_settings_bulk_upload_dialog_passwords,
                        localDataDescriptionsMap);
        // If the Passwords switch is shown, and the Bookmarks switch is shown above, then the
        // Passwords top separator should be shown.
        if (mPasswordsSwitch != null && mBookmarkSwitch != null) {
            view.findViewById(R.id.account_settings_bulk_upload_dialog_passwords_top_separator)
                    .setVisibility(View.VISIBLE);
        }

        mReadingListSwitch =
                updateDataTypeSwitch(
                        context,
                        view,
                        DataType.READING_LIST,
                        R.id.account_settings_bulk_upload_dialog_reading_list,
                        R.plurals.account_settings_bulk_upload_dialog_reading_list,
                        localDataDescriptionsMap);
        // If the Reading List switch is shown, and there's at least one switch shown above, then
        // the Reading List top separator should be shown.
        if (mReadingListSwitch != null && (mPasswordsSwitch != null || mBookmarkSwitch != null)) {
            view.findViewById(R.id.account_settings_bulk_upload_dialog_reading_list_top_separator)
                    .setVisibility(View.VISIBLE);
        }
    }

    private MaterialSwitchWithTitleAndSummary updateDataTypeSwitch(
            Context context,
            View view,
            int dataType,
            @IdRes int switchViewId,
            @PluralsRes int switchTextId,
            HashMap<Integer, LocalDataDescription> localDataDescriptionsMap) {
        LocalDataDescription typeLocalDataDescription = localDataDescriptionsMap.get(dataType);
        boolean shouldShowSwitch =
                typeLocalDataDescription != null && typeLocalDataDescription.itemCount() > 0;

        if (shouldShowSwitch) {
            MaterialSwitchWithTitleAndSummary typeSwitch =
                    (MaterialSwitchWithTitleAndSummary) view.findViewById(switchViewId);
            typeSwitch.setOnCheckedChangeListener(
                    (button, isChecked) -> {
                        mModel.set(
                                ModalDialogProperties.POSITIVE_BUTTON_DISABLED,
                                getCheckedTypes().isEmpty());
                    });
            typeSwitch.setVisibility(View.VISIBLE);
            typeSwitch.setTitleText(
                    context.getResources()
                            .getQuantityString(
                                    switchTextId,
                                    typeLocalDataDescription.itemCount(),
                                    typeLocalDataDescription.itemCount()));

            typeSwitch.setSummaryText(typeLocalDataDescription.getDomainsDisplayText());
            return typeSwitch;
        }
        return null;
    }

    private static View inflateView(Context context) {
        final View view =
                LayoutInflater.from(context).inflate(R.layout.batch_upload_dialog_view, null);

        return view;
    }

    private Boolean isTypeSwitchChecked(MaterialSwitchWithTitleAndSummary typeSwitch) {
        return typeSwitch.getVisibility() == View.VISIBLE && typeSwitch.isChecked();
    }

    private Set<Integer> getCheckedTypes() {
        Set<Integer> checkedTypes = new HashSet<>();
        if (mBookmarkSwitch != null && isTypeSwitchChecked(mBookmarkSwitch)) {
            checkedTypes.add(DataType.BOOKMARKS);
        }
        if (mPasswordsSwitch != null && isTypeSwitchChecked(mPasswordsSwitch)) {
            checkedTypes.add(DataType.PASSWORDS);
        }
        if (mReadingListSwitch != null && isTypeSwitchChecked(mReadingListSwitch)) {
            checkedTypes.add(DataType.READING_LIST);
        }
        return checkedTypes;
    }

    private int getItemsCount(
            Set<Integer> types, HashMap<Integer, LocalDataDescription> localDataDescriptionsMap) {
        int itemsCount = 0;
        for (int type : types) {
            itemsCount += localDataDescriptionsMap.get(type).itemCount();
        }
        return itemsCount;
    }

    private Controller createController(
            HashMap<Integer, LocalDataDescription> localDataDescriptionsMap, Listener listener) {
        return new Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {
                if (buttonType == ButtonType.POSITIVE) {
                    mDialogManager.dismissDialog(
                            mModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                    Set<Integer> types = getCheckedTypes();
                    listener.onSaveInAccountDialogButtonClicked(
                            types, getItemsCount(types, localDataDescriptionsMap));
                } else if (buttonType == ButtonType.NEGATIVE) {
                    mDialogManager.dismissDialog(
                            mModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                }
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {}
        };
    }
}
