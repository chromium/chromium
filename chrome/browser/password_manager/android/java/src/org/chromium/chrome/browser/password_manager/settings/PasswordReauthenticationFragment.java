// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static org.chromium.chrome.browser.password_manager.settings.PasswordAccessReauthenticationHelper.SETTINGS_REAUTHENTICATION_HISTOGRAM;

import android.app.Activity;
import android.app.KeyguardManager;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.device_reauth.ReauthResult;

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
            if (resultCode == Activity.RESULT_OK) {
                RecordHistogram.recordEnumeratedHistogram(
                        SETTINGS_REAUTHENTICATION_HISTOGRAM,
                        ReauthResult.SUCCESS,
                        ReauthResult.MAX_VALUE + 1);
                ReauthenticationManager.recordLastReauth(
                        System.currentTimeMillis(), getArguments().getInt(SCOPE_ID));
            } else {
                RecordHistogram.recordEnumeratedHistogram(
                        SETTINGS_REAUTHENTICATION_HISTOGRAM,
                        ReauthResult.FAILURE,
                        ReauthResult.MAX_VALUE + 1);
                ReauthenticationManager.resetLastReauth();
            }
            mFragmentManager.popBackStack();
        }
    }

    /** Prevent calling the {@link #lockDevice} method in {@link #onCreate}. */
    public static void preventLockingForTesting() {
        sPreventLockDevice = true;
    }

    private void lockDevice() {
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
