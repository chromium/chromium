// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.SynchronousInitializationActivity;
import org.chromium.chrome.browser.ui.signin.SyncConsentFragmentBase;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

/**
 * Allows the user to pick an account, sign in and enable sync. Started from Settings and various
 * sign-in promos. For more details see {@link SyncConsentFragmentBase}.
 */
public class SyncConsentActivity extends SynchronousInitializationActivity {
    private static final String ARGUMENT_FRAGMENT_ARGS = "SigninActivity.FragmentArgs";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.signin_activity);

        FragmentManager fragmentManager = getSupportFragmentManager();
        Fragment fragment = fragmentManager.findFragmentById(R.id.fragment_container);
        if (fragment == null) {
            Bundle fragmentArgs = getIntent().getBundleExtra(ARGUMENT_FRAGMENT_ARGS);
            fragment = new SyncConsentFragment();
            fragment.setArguments(fragmentArgs);
            fragmentManager.beginTransaction().add(R.id.fragment_container, fragment).commit();
        }
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        return new ModalDialogManager(new AppModalPresenter(this), ModalDialogType.APP);
    }

    /**
     * Creates a new intent to start the {@link SyncConsentActivity}.
     *
     * @param fragmentArgs arguments to create an {@link SyncConsentFragment}.
     */
    static Intent createIntent(Context context, Bundle fragmentArgs) {
        Intent intent = new Intent(context, SyncConsentActivity.class);
        intent.putExtra(ARGUMENT_FRAGMENT_ARGS, fragmentArgs);
        return intent;
    }
}
