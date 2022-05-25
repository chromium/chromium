// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static org.chromium.chrome.browser.incognito.reauth.IncognitoReauthProperties.createPropertyModel;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.incognito.R;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherCustomViewManager;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The coordinator which is responsible for showing the Incognito re-authentication page.
 *
 * This is created and removed each time the incognito re-auth screen is shown/hidden
 * respectively.
 *
 * TODO(crbug.com/1227656): Add support to disable/enable certain UI elements in the Toolbar when
 * the re-auth dialog is shown/hidden in the Incognito tab switcher.
 */
class IncognitoReauthCoordinator {
    private final @NonNull Context mContext;
    private final @NonNull ModalDialogManager mModalDialogManager;
    // This can be null for cases where tab switcher might not have been created yet.
    // This happens when user hasn't yet interacted with tab switcher but we create
    // this coordinator to display a full-screen dialog.
    private final @Nullable TabSwitcherCustomViewManager mTabSwitcherCustomViewManager;
    // This can be null if the context is not a tabbed Activity.
    private final @Nullable IncognitoReauthTopToolbarDelegate mIncognitoReauthTopToolbarDelegate;
    private final boolean mShowFullScreen;

    // Non-null for full screen re-auth dialog.
    private final @Nullable IncognitoReauthMenuDelegate mIncognitoReauthMenuDelegate;
    private final IncognitoReauthMediator mIncognitoReauthMediator;

    private View mIncognitoReauthView;
    private IncognitoReauthDialog mIncognitoReauthDialog;
    private PropertyModel mPropertyModel;
    private PropertyModelChangeProcessor mModelChangeProcessor;
    // A token returned by {@link IncognitoReauthTopToolbarDelegate} that needs to be used to
    // re-enable the new tab button control state.
    private @Nullable Integer mNewTabInteractabilityToken;

    /**
     * @param context The {@link Context} to use for inflating the Incognito re-auth view.
     * @param tabModelSelector The {@link TabModelSelector} which will be passed to the mediator in
     *         order to switch {@link TabModel} when the user clicks on "See other tabs" button.
     * @param modalDialogManager The {@link ModalDialogManager} which is used to fire the dialog
     *         containing the Incognito re-auth view.
     * @param incognitoReauthCallback The {@link IncognitoReauthCallback} which would be executed
     *         after an authentication attempt.
     * @param incognitoReauthManager The {@link IncognitoReauthManager} instance which would be used
     *         to initiate re-authentication.
     * @param settingsLauncher A {@link SettingsLauncher} that allows to fire {@link
     *         SettingsActivity}.
     * @param tabSwitcherCustomViewManager A {@link TabSwitcherCustomViewManager} that
     *         allows to communicate with tab switcher to show the re-auth screen.
     * @param incognitoReauthTopToolbarDelegate A {@link IncognitoReauthTopToolbarDelegate}
     *         responsible for controlling the interactability of the top toolbar elements.
     * @param showFullScreen Whether to show a fullscreen / tab based re-auth dialog.
     */
    public IncognitoReauthCoordinator(@NonNull Context context,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull IncognitoReauthManager.IncognitoReauthCallback incognitoReauthCallback,
            @NonNull IncognitoReauthManager incognitoReauthManager,
            @NonNull SettingsLauncher settingsLauncher,
            @Nullable TabSwitcherCustomViewManager tabSwitcherCustomViewManager,
            @Nullable IncognitoReauthTopToolbarDelegate incognitoReauthTopToolbarDelegate,
            boolean showFullScreen) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mTabSwitcherCustomViewManager = tabSwitcherCustomViewManager;
        mIncognitoReauthTopToolbarDelegate = incognitoReauthTopToolbarDelegate;
        mShowFullScreen = showFullScreen;
        mIncognitoReauthMediator = new IncognitoReauthMediator(
                tabModelSelector, incognitoReauthCallback, incognitoReauthManager);
        mIncognitoReauthMenuDelegate = (mShowFullScreen)
                ? new IncognitoReauthMenuDelegate(mContext, tabModelSelector, settingsLauncher)
                : null;
    }

    private void destroy() {
        mModelChangeProcessor.destroy();
    }

    // TODO(crbug.com/1227656): Rename the method to remove reference to Dialog.
    void showDialog() {
        mIncognitoReauthView =
                LayoutInflater.from(mContext).inflate(R.layout.incognito_reauth_view, null);
        ListMenuButtonDelegate delegate = (mShowFullScreen) ? ()
                -> mIncognitoReauthMenuDelegate.getBasicListMenu()
                : null;
        mPropertyModel = createPropertyModel(
                mIncognitoReauthMediator::onUnlockIncognitoButtonClicked,
                mIncognitoReauthMediator::onSeeOtherTabsButtonClicked, mShowFullScreen, delegate);
        mModelChangeProcessor = PropertyModelChangeProcessor.create(
                mPropertyModel, mIncognitoReauthView, IncognitoReauthViewBinder::bind);

        if (mShowFullScreen) {
            mIncognitoReauthDialog =
                    new IncognitoReauthDialog(mModalDialogManager, mIncognitoReauthView);
            mIncognitoReauthDialog.showIncognitoReauthDialog(mShowFullScreen);
        } else {
            assert mTabSwitcherCustomViewManager
                    != null : "TabSwitcherCustomViewManager can't be null.";
            boolean success = mTabSwitcherCustomViewManager.requestView(mIncognitoReauthView);
            assert success : "Unable to signal showing the re-auth screen to tab switcher.";

            assert mIncognitoReauthTopToolbarDelegate
                    != null : "delegate to top toolbar can't be null";
            mNewTabInteractabilityToken = mIncognitoReauthTopToolbarDelegate.disableNewTabButton();
        }
    }

    // TODO(crbug.com/1227656): Rename and possibly refactor the method to remove reference to
    // Dialog.
    void hideDialogAndDestroy(@DialogDismissalCause int dismissalCause) {
        if (mShowFullScreen) {
            assert mIncognitoReauthDialog != null : "Incognito re-auth dialog doesn't exists.";
            mIncognitoReauthDialog.dismissIncognitoReauthDialog(dismissalCause);
        } else {
            assert mTabSwitcherCustomViewManager
                    != null : "TabSwitcherCustomViewManager can't be null.";
            boolean success = mTabSwitcherCustomViewManager.releaseView();
            assert success : "Unable to signal removing the re-auth screen from tab switcher.";

            assert mIncognitoReauthTopToolbarDelegate
                    != null : "delegate to top toolbar can't be null";
            assert mNewTabInteractabilityToken != null : "Top toolbar manager was not acquired.";
            mIncognitoReauthTopToolbarDelegate.enableNewTabButton(mNewTabInteractabilityToken);
            mNewTabInteractabilityToken = null;
        }
        destroy();
    }
}
