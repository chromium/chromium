// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;

/**
 * Allows the user to pick an account, sign in and enable sync. Started from Settings and various
 * sign-in promos. For more details see {@link SyncConsentFragmentBase}.
 */
// TODO(https://crbug.com/820491): extend AsyncInitializationActivity.
public class SyncConsentActivity extends ChromeBaseAppCompatActivity {
    private static final String ARGUMENT_FRAGMENT_ARGS = "SigninActivity.FragmentArgs";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Make sure the native is initialized before calling super.onCreate(), as it might recreate
        // SyncConsentFragment that currently depends on native. See https://crbug.com/983730.
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();

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
