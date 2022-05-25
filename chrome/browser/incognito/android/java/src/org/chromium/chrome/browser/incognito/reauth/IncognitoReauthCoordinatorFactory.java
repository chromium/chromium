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
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherCustomViewManager;
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
    // This is non-null for Tabbed activity.
    private final @Nullable IncognitoReauthTopToolbarDelegate mIncognitoReauthTopToolbarDelegate;

    // A callback controller to monitor the availability of {@link TabSwitcherCustomViewManager}.
    private final CallbackController mTabSwitcherCustomViewManagerController =
            new CallbackController();

    // This is non-null for Tabbed Activity;
    private @Nullable TabSwitcherCustomViewManager mTabSwitcherCustomViewManager;

    /**
     * @param context The {@link Context} to use for inflating the Incognito re-auth view.
     * @param tabModelSelector The {@link TabModelSelector} which will be passed to the mediator in
     *         order to switch {@link TabModel} when the user clicks on "See other tabs" button.
     * @param modalDialogManager The {@link ModalDialogManager} which is used to fire the dialog
     *         containing the Incognito re-auth view.
     * @param  settingsLauncher A {@link SettingsLauncher} that allows to launch {@link
     *         SettingsActivity} from 3 dots menu.
     * @param tabSwitcherCustomViewManagerOneshotSupplier A {@link OneshotSupplier
     *         <TabSwitcherCustomViewManager>} that allows to communicate with tab switcher to
     *         show the re-auth screen.
     * @param topToolbarDelegate A {@link IncognitoReauthTopToolbarDelegate} responsible for
     *         controlling the interactability of the top toolbar elements.
     */
    public IncognitoReauthCoordinatorFactory(@NonNull Context context,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull SettingsLauncher settingsLauncher,
            @NonNull OneshotSupplier<TabSwitcherCustomViewManager>
                    tabSwitcherCustomViewManagerOneshotSupplier,
            @Nullable IncognitoReauthTopToolbarDelegate topToolbarDelegate) {
        mContext = context;
        mTabModelSelector = tabModelSelector;
        mModalDialogManager = modalDialogManager;
        mIncognitoReauthTopToolbarDelegate = topToolbarDelegate;
        mIncognitoReauthManager = new IncognitoReauthManager();
        mSettingsLauncher = settingsLauncher;
        tabSwitcherCustomViewManagerOneshotSupplier.onAvailable(
                mTabSwitcherCustomViewManagerController.makeCancelable(manager -> {
                    assert manager != null;
                    mTabSwitcherCustomViewManager = manager;
                }));
    }

    void destroy() {
        mTabSwitcherCustomViewManagerController.destroy();
    }

    IncognitoReauthCoordinator createIncognitoReauthCoordinator(
            @NonNull IncognitoReauthManager.IncognitoReauthCallback incognitoReauthCallback,
            boolean showFullScreen) {
        return new IncognitoReauthCoordinator(mContext, mTabModelSelector, mModalDialogManager,
                incognitoReauthCallback, mIncognitoReauthManager, mSettingsLauncher,
                mTabSwitcherCustomViewManager, mIncognitoReauthTopToolbarDelegate, showFullScreen);
    }
}
