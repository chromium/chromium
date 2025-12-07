// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager.IncognitoReauthCallback;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** The coordinator responsible for showing the full-screen re-auth dialog. */
@NullMarked
class FullScreenIncognitoReauthCoordinator extends IncognitoReauthCoordinatorBase {
    /** The manager responsible for showing the full screen Incognito re-auth dialog. */
    private final ModalDialogManager mModalDialogManager;

    /** The 3 dots menu delegate shown inside the full screen dialog. */
    private final IncognitoReauthMenuDelegate mIncognitoReauthMenuDelegate;

    /**
     * The callback which would be fired when the user press back while the re-auth dialog is shown.
     */
    private final OnBackPressedCallback mOnBackPressedCallback;

    /** The dialog which contains the logic to show the re-auth full-screen. */
    private @Nullable IncognitoReauthDialog mIncognitoReauthDialog;

    /** Test-only method to ignore the assertion check on the dialog. */
    @VisibleForTesting boolean mIgnoreDialogCreationForTesting;

    /**
     * @param context The {@link Context} to use for fetching the re-auth resources.
     * @param incognitoReauthManager The {@link IncognitoReauthManager} instance which would be used
     *     to initiate re-authentication.
     * @param incognitoReauthCallback The {@link IncognitoReauthCallback} which would be executed
     *     after an authentication attempt.
     * @param seeOtherTabsRunnable A {@link Runnable} which is run when the user clicks on "See
     *     other tabs" option.
     * @param modalDialogManager The {@link ModalDialogManager} which is used to fire the dialog
     *     containing the Incognito re-auth view.
     * @param incognitoReauthMenuDelegate The {@link IncognitoReauthMenuDelegate} responsible for
     * @param backPressedCallback The {@link OnBackPressedCallback} which would be called when a
     *     user presses back while the fullscreen re-auth is shown.
     */
    public FullScreenIncognitoReauthCoordinator(
            Context context,
            IncognitoReauthManager incognitoReauthManager,
            IncognitoReauthCallback incognitoReauthCallback,
            Runnable seeOtherTabsRunnable,
            ModalDialogManager modalDialogManager,
            IncognitoReauthMenuDelegate incognitoReauthMenuDelegate,
            OnBackPressedCallback backPressedCallback) {
        super(context, incognitoReauthManager, incognitoReauthCallback, seeOtherTabsRunnable);
        mOnBackPressedCallback = backPressedCallback;
        mModalDialogManager = modalDialogManager;
        mIncognitoReauthMenuDelegate = incognitoReauthMenuDelegate;
    }

    /** A method to show the Incognito re-auth dialog. */
    @Override
    public void show() {
        prepareToShow(mIncognitoReauthMenuDelegate.getListMenuDelegate(), /* fullscreen= */ true);
        // TODO(crbug.com/40056462): Find a cleaner way to test.
        if (!mIgnoreDialogCreationForTesting) {
            assert mIncognitoReauthDialog == null : "Incognito re-auth dialog already exists.";
            mIncognitoReauthDialog =
                    new IncognitoReauthDialog(
                            mModalDialogManager, getIncognitoReauthView(), mOnBackPressedCallback);
        }
        assumeNonNull(mIncognitoReauthDialog);
        mIncognitoReauthDialog.showIncognitoReauthDialog();
    }

    /**
     * A method to hide the incognito re-auth dialog.
     *
     * @param dismissalCause The {@link DialogDismissalCause} for the dismissal of the re-auth
     *         screen.
     */
    @Override
    public void hide(@DialogDismissalCause int dismissalCause) {
        assert mIncognitoReauthDialog != null;
        mIncognitoReauthDialog.dismissIncognitoReauthDialog(dismissalCause);
        destroy();
    }

    /** A test-only method to set a mock {@link IncognitoReauthDialog}. */
    void setIncognitoReauthDialogForTesting(IncognitoReauthDialog incognitoReauthDialog) {
        mIncognitoReauthDialog = incognitoReauthDialog;
    }
}
