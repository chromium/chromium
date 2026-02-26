// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_lock;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.CallSuper;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.SynchronousInitializationActivity;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.device_lock.DeviceLockCoordinator;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Informs the user on using a device lock to protect their privacy and data on the device. If the
 * device does not currently have a device lock, the user will be prompted to create one.
 */
@NullMarked
public class DeviceLockActivity extends SynchronousInitializationActivity
        implements DeviceLockCoordinator.Delegate {
    private static final String ARGUMENT_FRAGMENT_ARGS = "DeviceLockActivity.FragmentArgs";
    private static final String ARGUMENT_SELECTED_ACCOUNT_GAIA_ID =
            "DeviceLockActivity.FragmentArgs.SelectedAccountGaiaId";
    private static final String ARGUMENT_SOURCE = "DeviceLockActivity.FragmentArgs.Source";
    private static final String ARGUMENT_REQUIRE_DEVICE_LOCK_REAUTHENTICATION =
            "DeviceLockActivity.FragmentArgs.RequireDeviceLockReauthentication";

    private FrameLayout mFrameLayout;
    private WindowAndroid mWindowAndroid;
    private @Nullable IntentRequestTracker mIntentRequestTracker;
    private DeviceLockCoordinator mDeviceLockCoordinator;

    @Override
    public void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        if (mIntentRequestTracker != null) {
            mIntentRequestTracker.onActivityResult(requestCode, resultCode, assumeNonNull(data));
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    void setIntentRequestTrackerForTesting(IntentRequestTracker intentRequestTracker) {
        mIntentRequestTracker = intentRequestTracker;
    }

    @Override
    @Initializer
    protected void onProfileAvailable(Profile profile) {
        super.onProfileAvailable(profile);
        mFrameLayout = new FrameLayout(this);
        setContentView(mFrameLayout);
        mWindowAndroid =
                new ActivityWindowAndroid(
                        this,
                        /* listenToActivityState= */ true,
                        IntentRequestTracker.createFromActivity(this),
                        getInsetObserver(),
                        /* trackOcclusion= */ true);
        mIntentRequestTracker = mWindowAndroid.getIntentRequestTracker();

        Bundle fragmentArgs = getIntent().getBundleExtra(ARGUMENT_FRAGMENT_ARGS);
        assumeNonNull(fragmentArgs);
        @Nullable String selectedAccountGaiaId =
                fragmentArgs.getString(ARGUMENT_SELECTED_ACCOUNT_GAIA_ID, null);
        @Nullable CoreAccountId selectedAccountId =
                selectedAccountGaiaId == null
                        ? null
                        : new CoreAccountId(new GaiaId(selectedAccountGaiaId));
        boolean requireDeviceLockReauthentication =
                fragmentArgs.getBoolean(ARGUMENT_REQUIRE_DEVICE_LOCK_REAUTHENTICATION, true);

        assert profile != null;
        ReauthenticatorBridge reauthenticatorBridge =
                requireDeviceLockReauthentication
                        ? DeviceLockCoordinator.createDeviceLockAuthenticatorBridge(this, profile)
                        : null;
        mDeviceLockCoordinator =
                new DeviceLockCoordinator(
                        this, mWindowAndroid, reauthenticatorBridge, this, selectedAccountId);
    }

    @CallSuper
    @Override
    protected void onDestroy() {
        mWindowAndroid.destroy();
        mDeviceLockCoordinator.destroy();
        super.onDestroy();
    }

    @Override
    protected @Nullable ModalDialogManager createModalDialogManager() {
        return null;
    }

    protected static Bundle createArguments(
            @Nullable CoreAccountId selectedAccountId,
            @DeviceLockActivityLauncher.Source String source,
            boolean requireDeviceLockReauthentication) {
        Bundle result = new Bundle();
        if (selectedAccountId != null) {
            result.putString(
                    ARGUMENT_SELECTED_ACCOUNT_GAIA_ID, selectedAccountId.getId().toString());
        }
        result.putString(ARGUMENT_SOURCE, source);
        result.putBoolean(
                ARGUMENT_REQUIRE_DEVICE_LOCK_REAUTHENTICATION, requireDeviceLockReauthentication);
        return result;
    }

    /** Creates a new intent to start the {@link DeviceLockActivity}. */
    protected static Intent createIntent(
            Context context,
            @Nullable CoreAccountId selectedAccountId,
            boolean requireDeviceLockReauthentication,
            @DeviceLockActivityLauncher.Source String source) {
        Intent intent = new Intent(context, DeviceLockActivity.class);
        intent.putExtra(
                ARGUMENT_FRAGMENT_ARGS,
                DeviceLockActivity.createArguments(
                        selectedAccountId, source, requireDeviceLockReauthentication));
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

    @Override
    public @DeviceLockActivityLauncher.Source String getSource() {
        Bundle fragmentArgs = getIntent().getBundleExtra(ARGUMENT_FRAGMENT_ARGS);
        assumeNonNull(fragmentArgs);
        return assertNonNull(fragmentArgs.getString(ARGUMENT_SOURCE));
    }
}
