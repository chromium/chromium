// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.search_engines.settings.custom_site_search;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
public class AddSearchEngineDialogCoordinator {
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final TemplateUrlService mTemplateUrlService;
    @Nullable private PropertyModel mDialogModel;

    public AddSearchEngineDialogCoordinator(
            Context context,
            ModalDialogManager modalDialogManager,
            TemplateUrlService templateUrlService) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mTemplateUrlService = templateUrlService;
    }

    public void show() {
        LayoutInflater inflater = LayoutInflater.from(mContext);
        View view = inflater.inflate(R.layout.site_search_dialog, null);
        EditText nameInput = view.findViewById(R.id.name_input);
        EditText shortcutInput = view.findViewById(R.id.shortcut_input);
        EditText urlInput = view.findViewById(R.id.url_input);

        ModalDialogProperties.Controller controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                            String name = nameInput.getText().toString();
                            String keyword = shortcutInput.getText().toString();
                            String url = urlInput.getText().toString();
                            // TODO: handle user input validation after C++ logic is ported
                            mTemplateUrlService.addSearchEngine(name, keyword, url);
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

        String dialogTitle = mContext.getString(R.string.site_search_dialog_title_add);
        String saveText = mContext.getString(R.string.site_search_dialog_add);
        String cancelText = mContext.getString(R.string.site_search_dialog_cancel);

        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.TITLE, dialogTitle)
                        .with(ModalDialogProperties.CUSTOM_VIEW, view)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, saveText)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, cancelText)
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
