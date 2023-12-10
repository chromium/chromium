// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.app.Activity;
import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.WindowAndroid;

/**
 * A controller that triggers an auto sign-in snackbar. Auto sign-in snackbar is
 * triggered on a request credentials call of a Credential Management API.
 */
public class AutoSigninSnackbarController implements SnackbarManager.SnackbarController {
    private final SnackbarManager mSnackbarManager;
    private final TabObserver mTabObserver;
    private final Tab mTab;

    /**
     * Displays Auto sign-in snackbar, which communicates to the users that they
     * were signed in to the web site.
     */
    @CalledByNative
    private static void showSnackbar(Tab tab, String text) {
        Activity activity = TabUtils.getActivity(tab);
        if (activity == null) return;
        WindowAndroid windowAndroid = tab.getWindowAndroid();
        if (windowAndroid == null) return;
        SnackbarManager snackbarManager = SnackbarManagerProvider.from(windowAndroid);
        AutoSigninSnackbarController snackbarController =
                new AutoSigninSnackbarController(snackbarManager, tab);
        Snackbar snackbar =
                Snackbar.make(
                        text,
                        snackbarController,
                        Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_AUTO_LOGIN);
        int backgroundColor = SemanticColorUtils.getDefaultControlColorActive(activity);
        Drawable icon = AppCompatResources.getDrawable(activity, R.drawable.logo_avatar_anonymous);
        snackbar.setSingleLine(false)
                .setBackgroundColor(backgroundColor)
                .setProfileImage(icon)
                .setTextAppearance(R.style.TextAppearance_TextMedium_Primary_Baseline_Light);
        snackbarManager.showSnackbar(snackbar);
    }

    /**
     * Creates an instance of a {@link AutoSigninSnackbarController}.
     * @param snackbarManager The manager that helps to show up snackbar.
     */
    private AutoSigninSnackbarController(SnackbarManager snackbarManager, Tab tab) {
        mTab = tab;
        mSnackbarManager = snackbarManager;
        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onHidden(Tab tab, @TabHidingType int type) {
                        AutoSigninSnackbarController.this.dismissAutoSigninSnackbar();
                    }

                    @Override
                    public void onDestroyed(Tab tab) {
                        AutoSigninSnackbarController.this.dismissAutoSigninSnackbar();
                    }

                    @Override
                    public void onCrash(Tab tab) {
                        AutoSigninSnackbarController.this.dismissAutoSigninSnackbar();
                    }
                };
        mTab.addObserver(mTabObserver);
    }

    /** Dismisses the snackbar. */
    public void dismissAutoSigninSnackbar() {
        if (mSnackbarManager.isShowing()) {
            mSnackbarManager.dismissSnackbars(this);
        }
    }

    @Override
    public void onAction(Object actionData) {}

    @Override
    public void onDismissNoAction(Object actionData) {
        mTab.removeObserver(mTabObserver);
    }
}
