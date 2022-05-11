// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * A factory to create an {@link IncognitoReauthCoordinator} instance.
 */
public class IncognitoReauthCoordinatorFactory {
    private final @NonNull Context mContext;
    private final @NonNull TabModelSelector mTabModelSelector;
    private final @NonNull ModalDialogManager mModalDialogManager;
    private final @NonNull IncognitoReauthManager mIncognitoReauthManager;
    private final @NonNull SettingsLauncher mSettingsLauncher;

    // A callback controller for initializing the |mIncognitoReauthTabSwitcherDelegate| which is
    // invoked once the {@link TabSwitcherCustomViewManager} is initiated and created.
    private final CallbackController mTabSwitcherDelegateController = new CallbackController();
    private @Nullable IncognitoReauthTabSwitcherDelegate mIncognitoReauthTabSwitcherDelegate;

    /**
     * @param context The {@link Context} to use for inflating the Incognito re-auth view.
     * @param tabModelSelector The {@link TabModelSelector} which will be passed to the mediator in
     *         order to switch {@link TabModel} when the user clicks on "See other tabs" button.
     * @param modalDialogManager The {@link ModalDialogManager} which is used to fire the dialog
     *         containing the Incognito re-auth view.
     * @param  settingsLauncher A {@link SettingsLauncher} that allows to launch {@link
     *         SettingsActivity} from 3 dots menu.
     * @param incognitoReauthTabSwitcherDelegateSupplier A {@link OneshotSupplier
     *         <IncognitoReauthTabSwitcherDelegate>} that allows to communicate with tab switcher to
     *         show the re-auth screen.
     */
    public IncognitoReauthCoordinatorFactory(@NonNull Context context,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull SettingsLauncher settingsLauncher,
            @NonNull OneshotSupplier<IncognitoReauthTabSwitcherDelegate>
                    incognitoReauthTabSwitcherDelegateSupplier) {
        mContext = context;
        mTabModelSelector = tabModelSelector;
        mModalDialogManager = modalDialogManager;
        mIncognitoReauthManager = new IncognitoReauthManager();
        mSettingsLauncher = settingsLauncher;
        incognitoReauthTabSwitcherDelegateSupplier.onAvailable(
                mTabSwitcherDelegateController.makeCancelable(delegate -> {
                    assert delegate != null;
                    mIncognitoReauthTabSwitcherDelegate = delegate;
                }));
    }

    void destroy() {
        mTabSwitcherDelegateController.destroy();
    }

    IncognitoReauthCoordinator createIncognitoReauthCoordinator(
            @NonNull IncognitoReauthManager.IncognitoReauthCallback incognitoReauthCallback,
            boolean showFullScreen) {
        return new IncognitoReauthCoordinator(mContext, mTabModelSelector, mModalDialogManager,
                incognitoReauthCallback, mIncognitoReauthManager, mSettingsLauncher,
                mIncognitoReauthTabSwitcherDelegate, showFullScreen);
    }
}
