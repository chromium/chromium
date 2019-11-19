// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.password;

import android.annotation.TargetApi;
import android.app.KeyguardManager;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;

import androidx.annotation.VisibleForTesting;

/** Show the lock screen confirmation and lock the screen. */
public class PasswordReauthenticationFragment extends Fragment {
    /**
     * The key for the description argument, which is used to retrieve an explanation of the
     * reauthentication prompt to the user.
     */
    public static final String DESCRIPTION_ID = "description";

    /**
     * The key for the scope, with values from {@link ReauthenticationManager.ReauthScope}. The
     * scope enum value corresponds to what is indicated in the description message for the user
     * (e.g., if the message mentions "export passwords", the scope should be BULK, but for "view
     * password" it should be ONE_AT_A_TIME).
     */
    public static final String SCOPE_ID = "scope";

    protected static final int CONFIRM_DEVICE_CREDENTIAL_REQUEST_CODE = 2;

    protected static final String HAS_BEEN_SUSPENDED_KEY = "has_been_suspended";

    private static boolean sPreventLockDevice;

    private FragmentManager mFragmentManager;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mFragmentManager = getFragmentManager();
        boolean isFirstTime = savedInstanceState == null;
        if (!sPreventLockDevice && isFirstTime) {
            lockDevice();
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        // On Android L, an empty |outState| would degrade to null in |onCreate|, making Chrome
        // unable to distinguish the first time launch. Insert a value into |outState| to prevent
        // that.
        outState.putBoolean(HAS_BEEN_SUSPENDED_KEY, true);
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == CONFIRM_DEVICE_CREDENTIAL_REQUEST_CODE) {
            if (resultCode == getActivity().RESULT_OK) {
                ReauthenticationManager.recordLastReauth(
                        System.currentTimeMillis(), getArguments().getInt(SCOPE_ID));
            } else {
                ReauthenticationManager.resetLastReauth();
            }
            mFragmentManager.popBackStack();
        }
    }

    /**
     * Prevent calling the {@link #lockDevice} method in {@link #onCreate}.
     */
    @VisibleForTesting
    public static void preventLockingForTesting() {
        sPreventLockDevice = true;
    }

    /**
     * Should only be called on Lollipop or above devices.
     */
    @TargetApi(VERSION_CODES.LOLLIPOP)
    private void lockDevice() {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP;
        KeyguardManager keyguardManager =
                (KeyguardManager) getActivity().getSystemService(Context.KEYGUARD_SERVICE);
        final int resourceId = getArguments().getInt(DESCRIPTION_ID, 0);
        // Forgetting to set the DESCRIPTION_ID is an error on the callsite.
        assert resourceId != 0;
        // Set title to null to use the system default title which is adapted to the particular type
        // of device lock which the user set up.
        Intent intent =
                keyguardManager.createConfirmDeviceCredentialIntent(null, getString(resourceId));
        if (intent != null) {
            startActivityForResult(intent, CONFIRM_DEVICE_CREDENTIAL_REQUEST_CODE);
            return;
        }
        mFragmentManager.popBackStackImmediate();
    }
}
