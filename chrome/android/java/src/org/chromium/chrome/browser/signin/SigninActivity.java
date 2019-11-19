// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;

import androidx.annotation.IntDef;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.ManagedPreferencesUtils;
import org.chromium.components.signin.metrics.SigninAccessPoint;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Allows user to pick an account and sign in. Started from Settings and various sign-in promos.
 */
// TODO(https://crbug.com/820491): extend AsyncInitializationActivity.
public class SigninActivity extends ChromeBaseAppCompatActivity {
    static final String ARGUMENT_FRAGMENT_ARGS = "SigninActivity.FragmentArgs";

    @IntDef({SigninAccessPoint.SETTINGS, SigninAccessPoint.BOOKMARK_MANAGER,
            SigninAccessPoint.RECENT_TABS, SigninAccessPoint.SIGNIN_PROMO,
            SigninAccessPoint.NTP_CONTENT_SUGGESTIONS, SigninAccessPoint.AUTOFILL_DROPDOWN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AccessPoint {}

    /**
     * A convenience method to create a SigninActivity passing the access point in the
     * intent. Checks if the sign in flow can be started before showing the activity.
     * @param accessPoint {@link AccessPoint} for starting signin flow. Used in metrics.
     * @return {@code true} if sign in has been allowed.
     */
    // TODO(https://crbug.com/1017697): Move this method to SigninActivityLauncher
    public static boolean startIfAllowed(Context context, @AccessPoint int accessPoint) {
        SigninManager signinManager = IdentityServicesProvider.getSigninManager();
        if (!signinManager.isSignInAllowed()) {
            if (signinManager.isSigninDisabledByPolicy()) {
                ManagedPreferencesUtils.showManagedByAdministratorToast(context);
            }
            return false;
        }
        SigninActivityLauncher.get().launchActivity(context, accessPoint);
        return true;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Make sure the native is initialized before calling super.onCreate(), as it might recreate
        // SigninFragment that currently depends on native. See https://crbug.com/983730.
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();

        super.onCreate(savedInstanceState);
        setContentView(R.layout.signin_activity);

        FragmentManager fragmentManager = getSupportFragmentManager();
        Fragment fragment = fragmentManager.findFragmentById(R.id.fragment_container);
        if (fragment == null) {
            Bundle fragmentArgs = getIntent().getBundleExtra(ARGUMENT_FRAGMENT_ARGS);
            fragment = Fragment.instantiate(this, SigninFragment.class.getName(), fragmentArgs);
            fragmentManager.beginTransaction().add(R.id.fragment_container, fragment).commit();
        }
    }
}
