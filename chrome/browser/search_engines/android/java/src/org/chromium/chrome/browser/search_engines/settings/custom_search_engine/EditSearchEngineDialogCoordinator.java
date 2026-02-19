// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.search_engines.settings.custom_search_engine;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
public class EditSearchEngineDialogCoordinator {
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    @Nullable private PropertyModel mDialogModel;

    public EditSearchEngineDialogCoordinator(
            Context context, ModalDialogManager modalDialogManager) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
    }

    public void show(TemplateUrl templateUrl) {
        LayoutInflater inflater = LayoutInflater.from(mContext);
        View view = inflater.inflate(R.layout.edit_site_search_dialog, null);
        EditText nameInput = view.findViewById(R.id.name_input);
        EditText shortcutInput = view.findViewById(R.id.shortcut_input);
        EditText urlInput = view.findViewById(R.id.url_input);

        nameInput.setText(templateUrl.getShortName());
        shortcutInput.setText(templateUrl.getKeyword());
        urlInput.setText(templateUrl.getURL());

        if (templateUrl.getPrepopulatedId() > 0) {
            urlInput.setEnabled(false);
        }

        ModalDialogProperties.Controller controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                            // TODO: save changes via TemplateUrlService using templateUrl

                            mModalDialogManager.dismissDialog(
                                    model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                        } else {
                            mModalDialogManager.dismissDialog(
                                    model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                        }
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        mDialogModel = null;
                    }
                };

        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        // TODO: Replace with Android strings.grd
                        .with(ModalDialogProperties.TITLE, "Edit site search")
                        .with(ModalDialogProperties.CUSTOM_VIEW, view)
                        // TODO: Replace with Android strings.grd
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, "Save")
                        // TODO: Replace with Android strings.grd
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, "Cancel")
                        .build();

        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    public void dismiss() {
        if (mDialogModel != null) {
            mModalDialogManager.dismissDialog(
                    mDialogModel, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        }
    }
}
