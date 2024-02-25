// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.DETAILS;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.HELP_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.ILLUSTRATION;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.TITLE;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The coordinator for the password manager illustration modal dialog. Manages the sub-component
 * objects.
 */
public class PasswordManagerDialogCoordinator {
    private final PasswordManagerDialogMediator mMediator;
    private PropertyModel mModel;

    public PasswordManagerDialogCoordinator(
            ModalDialogManager modalDialogManager,
            View androidContentView,
            BrowserControlsStateProvider browserControlsStateProvider) {
        mMediator =
                new PasswordManagerDialogMediator(
                        new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS),
                        modalDialogManager,
                        androidContentView,
                        browserControlsStateProvider);
    }

    public void initialize(Context context, PasswordManagerDialogContents contents) {
        View customView =
                contents.getHelpButtonCallback() != null
                        ? LayoutInflater.from(context)
                                .inflate(R.layout.password_manager_dialog_with_help_button, null)
                        : LayoutInflater.from(context)
                                .inflate(R.layout.password_manager_dialog, null);
        mModel = buildModel(contents);
        mMediator.initialize(mModel, customView, contents);
        PropertyModelChangeProcessor.create(
                mModel, customView, PasswordManagerDialogViewBinder::bind);
    }

    public void showDialog() {
        mMediator.showDialog();
    }

    public void dismissDialog(@DialogDismissalCause int dismissalCause) {
        mMediator.dismissDialog(dismissalCause);
    }

    private PropertyModel buildModel(PasswordManagerDialogContents contents) {
        return PasswordManagerDialogProperties.defaultModelBuilder()
                .with(TITLE, contents.getTitle())
                .with(DETAILS, contents.getDetails())
                .with(ILLUSTRATION, contents.getIllustrationId())
                .with(HELP_BUTTON_CALLBACK, contents.getHelpButtonCallback())
                .build();
    }

    public PasswordManagerDialogMediator getMediatorForTesting() {
        return mMediator;
    }
}
