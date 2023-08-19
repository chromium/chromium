// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_lock;

import android.accounts.Account;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.SynchronousInitializationActivity;
import org.chromium.chrome.browser.ui.device_lock.DeviceLockCoordinator;
import org.chromium.components.signin.AccountUtils;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Informs the user on using a device lock to protect their privacy and data on the device. If
 * the device does not currently have a device lock, the user will be prompted to create one.
 */
public class DeviceLockActivity
        extends SynchronousInitializationActivity implements DeviceLockCoordinator.Delegate {
    private static final String ARGUMENT_FRAGMENT_ARGS = "DeviceLockActivity.FragmentArgs";
    private static final String ARGUMENT_SELECTED_ACCOUNT =
            "DeviceLockActivity.FragmentArgs.SelectedAccount";

    private FrameLayout mFrameLayout;
    private WindowAndroid mWindowAndroid;
    private IntentRequestTracker mIntentRequestTracker;
    private DeviceLockCoordinator mDeviceLockCoordinator;

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (mIntentRequestTracker != null) {
            mIntentRequestTracker.onActivityResult(requestCode, resultCode, data);
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    void setIntentRequestTrackerForTesting(IntentRequestTracker intentRequestTracker) {
        mIntentRequestTracker = intentRequestTracker;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mFrameLayout = new FrameLayout(this);
        setContentView(mFrameLayout);
        mWindowAndroid = new ActivityWindowAndroid(this, /* listenToActivityState= */ true,
                IntentRequestTracker.createFromActivity(this));
        mIntentRequestTracker = mWindowAndroid.getIntentRequestTracker();

        Bundle fragmentArgs = getIntent().getBundleExtra(ARGUMENT_FRAGMENT_ARGS);
        @Nullable
        String selectedAccountName = fragmentArgs.getString(ARGUMENT_SELECTED_ACCOUNT, null);
        @Nullable
        Account selectedAccount = selectedAccountName != null
                ? AccountUtils.createAccountFromName(selectedAccountName)
                : null;
        mDeviceLockCoordinator =
                new DeviceLockCoordinator(this, mWindowAndroid, this, selectedAccount);
    }

    @CallSuper
    @Override
    protected void onDestroy() {
        mWindowAndroid.destroy();
        mDeviceLockCoordinator.destroy();
        super.onDestroy();
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        return null;
    }

    protected static Bundle createArguments(@Nullable String selectedAccount) {
        Bundle result = new Bundle();
        result.putString(ARGUMENT_SELECTED_ACCOUNT, selectedAccount);
        return result;
    }

    /**
     * Creates a new intent to start the {@link DeviceLockActivity}.
     */
    protected static Intent createIntent(Context context, @Nullable String selectedAccount) {
        Intent intent = new Intent(context, DeviceLockActivity.class);
        intent.putExtra(
                ARGUMENT_FRAGMENT_ARGS, DeviceLockActivity.createArguments(selectedAccount));
        return intent;
    }

    @Override
    public void setView(View view) {
        mFrameLayout.removeAllViews();
        mFrameLayout.addView(view);
    }

    @Override
    public void onDeviceLockReady() {
        Intent intent = new Intent();
        setResult(Activity.RESULT_OK, intent);
        finish();
    }

    @Override
    public void onDeviceLockRefused() {
        Intent intent = new Intent();
        setResult(Activity.RESULT_CANCELED, intent);
        finish();
    }
}
