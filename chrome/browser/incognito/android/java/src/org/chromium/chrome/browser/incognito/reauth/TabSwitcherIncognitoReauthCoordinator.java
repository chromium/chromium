// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager.IncognitoReauthCallback;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherCustomViewManager;
import org.chromium.ui.modaldialog.DialogDismissalCause;

/**
 * The coordinator responsible for showing the tab-switcher re-auth screen.
 */
class TabSwitcherIncognitoReauthCoordinator extends IncognitoReauthCoordinatorBase {
    /** A manager which allows to pass the re-auth view to the tab switcher.*/
    private final @NonNull TabSwitcherCustomViewManager mTabSwitcherCustomViewManager;
    /** A delegate which allows to disable/enable top toolbar elements when re-auth is shown. */
    private final @NonNull IncognitoReauthTopToolbarDelegate mIncognitoReauthTopToolbarDelegate;
    /** The {@link Context} to use for fetching Incognito re-auth resources. */
    private final @NonNull Context mContext;
    /** A runnable to handle back presses which shows the regular overview mode. */
    private final @NonNull Runnable mBackPressRunnable;
    /**
     * A token returned by {@link IncognitoReauthTopToolbarDelegate} that needs to be used to
     * re-enable the new tab button control state.
     */
    private @Nullable Integer mNewTabInteractabilityToken;

    /**
     * @param context The {@link Context} to use for fetching the re-auth resources.
     * @param incognitoReauthManager The {@link IncognitoReauthManager} instance which would be
     *                              used to initiate re-authentication.
     * @param incognitoReauthCallback The {@link IncognitoReauthCallback} which would be executed
     *                               after an authentication attempt.
     * @param seeOtherTabsRunnable A {@link Runnable} which is run when the user clicks on
     *                            "See other tabs" option.
     * @param backPressRunnable A {@link Runnable} which is run when the user presses back while the
     *         re-auth view is shown.
     * @param tabSwitcherCustomViewManager A {@link TabSwitcherCustomViewManager} which allows to
     *         pass the re-auth view to the tab switcher
     * @param incognitoReauthTopToolbarDelegate A {@link IncognitoReauthTopToolbarDelegate} which
     */
    public TabSwitcherIncognitoReauthCoordinator(@NonNull Context context,
            @NonNull IncognitoReauthManager incognitoReauthManager,
            @NonNull IncognitoReauthCallback incognitoReauthCallback,
            @NonNull Runnable seeOtherTabsRunnable, @NonNull Runnable backPressRunnable,
            @NonNull TabSwitcherCustomViewManager tabSwitcherCustomViewManager,
            @NonNull IncognitoReauthTopToolbarDelegate incognitoReauthTopToolbarDelegate) {
        super(context, incognitoReauthManager, incognitoReauthCallback, seeOtherTabsRunnable);
        mContext = context;
        mTabSwitcherCustomViewManager = tabSwitcherCustomViewManager;
        mIncognitoReauthTopToolbarDelegate = incognitoReauthTopToolbarDelegate;
        mBackPressRunnable = backPressRunnable;
    }

    /**
     * A method to show the Incognito re-auth dialog.
     */
    @Override
    public void show() {
        prepareToshow(/*menuButtonDelegate= */ null, /*fullscreen= */ false);
        boolean success = mTabSwitcherCustomViewManager.requestView(
                getIncognitoReauthView(), mBackPressRunnable, /*clearTabList=*/true);
        assert success : "Unable to signal showing the re-auth screen to tab switcher.";
        mNewTabInteractabilityToken = mIncognitoReauthTopToolbarDelegate.disableNewTabButton();
    }

    /**
     * A method to hide the incognito re-auth dialog.
     *
     * @param dismissalCause The {@link DialogDismissalCause} for the dismissal of the re-auth
     *         screen.
     */
    @Override
    public void hide(@DialogDismissalCause int dismissalCause) {
        boolean success = mTabSwitcherCustomViewManager.releaseView();
        assert success : "Unable to signal removing the re-auth screen from tab switcher.";

        assert mNewTabInteractabilityToken != null : "Top toolbar manager was not acquired.";
        mIncognitoReauthTopToolbarDelegate.enableNewTabButton(mNewTabInteractabilityToken);
        mNewTabInteractabilityToken = null;

        destroy();
    }

    /**
     * A test-only method for setting the new tab interactability token.
     */
    protected void setNewTabInteractabilityTokenForTesting(@NonNull Integer token) {
        mNewTabInteractabilityToken = token;
    }
}