// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static org.chromium.chrome.browser.incognito.reauth.IncognitoReauthProperties.createPropertyModel;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.incognito.R;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The coordinator which is responsible for showing the Incognito re-authentication page.
 *
 * TODO(crbug.com/1227656): Add support to disable/enable certain UI elements in the Toolbar when
 * the re-auth dialog is shown/hidden in the Incognito tab switcher.
 */
class IncognitoReauthCoordinator {
    private final @NonNull Context mContext;
    private final @NonNull ModalDialogManager mModalDialogManager;
    private final boolean mShowFullScreen;

    private final IncognitoReauthMediator mIncognitoReauthMediator;

    private View mIncognitoReauthView;
    private IncognitoReauthDialog mIncognitoReauthDialog;
    private PropertyModel mPropertyModel;
    private PropertyModelChangeProcessor mModelChangeProcessor;

    /**
     * @param context The {@link Context} to use for inflating the Incognito re-auth view.
     * @param tabModelSelector The {@link TabModelSelector} which will be passed to the mediator in
     *         order to switch {@link TabModel} when the user clicks on "See other tabs" button.
     * @param modalDialogManager The {@link ModalDialogManager} which is used to fire the dialog
     *         containing the Incognito re-auth view.
     * @param incognitoReauthCallback The {@link IncognitoReauthCallback} which would be executed
     *         after an authentication attempt.
     * @param incognitoReauthManager The {@link IncognitoReauthManager} instance which would be used
     *                               to initiate re-authentication.
     * @param showFullScreen Whether to show a fullscreen / tab based re-auth dialog.
     */
    public IncognitoReauthCoordinator(@NonNull Context context,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull IncognitoReauthManager.IncognitoReauthCallback incognitoReauthCallback,
            @NonNull IncognitoReauthManager incognitoReauthManager, boolean showFullScreen) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mShowFullScreen = showFullScreen;
        mIncognitoReauthMediator = new IncognitoReauthMediator(
                tabModelSelector, incognitoReauthCallback, incognitoReauthManager);
    }

    private void destroy() {
        mModelChangeProcessor.destroy();
    }

    void showDialog() {
        mIncognitoReauthView =
                LayoutInflater.from(mContext).inflate(R.layout.incognito_reauth_view, null);
        mPropertyModel =
                createPropertyModel(mIncognitoReauthMediator::onUnlockIncognitoButtonClicked,
                        mIncognitoReauthMediator::onSeeOtherTabsButtonClicked, mShowFullScreen);
        mModelChangeProcessor = PropertyModelChangeProcessor.create(
                mPropertyModel, mIncognitoReauthView, IncognitoReauthViewBinder::bind);

        // TODO(crbug.com/1227656): Add implementation for R.id.incognito_reauth_menu_button
        mIncognitoReauthDialog =
                new IncognitoReauthDialog(mModalDialogManager, mIncognitoReauthView);
        mIncognitoReauthDialog.showIncognitoReauthDialog(mShowFullScreen);
    }

    void hideDialogAndDestroy(@DialogDismissalCause int dismissalCause) {
        assert mIncognitoReauthDialog != null : "Incognito re-auth dialog doesn't exists.";
        mIncognitoReauthDialog.dismissIncognitoReauthDialog(dismissalCause);
        destroy();
    }
}
