// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.compat.ApiHelperForN;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.CachedMetrics;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.widget.Toast;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Fallback {@link VrDelegate} implementation if the VR module is not available.
 */
/* package */ class VrDelegateFallback extends VrDelegate {
    /* package */ static final CachedMetrics
            .BooleanHistogramSample ENTER_VR_BROWSER_WITHOUT_FEATURE_MODULE_METRIC =
            new CachedMetrics.BooleanHistogramSample("VR.EnterVrBrowserWithoutFeatureModule");
    private static final String TAG = "VrDelegateFallback";
    private static final boolean DEBUG_LOGS = false;
    private static final String DEFAULT_VR_MODE_PACKAGE = "com.google.vr.vrcore";
    private static final String DEFAULT_VR_MODE_CLASS =
            "com.google.vr.vrcore.common.VrCoreListenerService";
    private static final int WAITING_FOR_MODULE_TIMEOUT_MS = 1500;

    @Override
    public void forceExitVrImmediately() {}

    @Override
    public boolean onActivityResultWithNative(int requestCode, int resultCode) {
        return false;
    }

    @Override
    public void onNativeLibraryAvailable() {}

    @Override
    public boolean isInVr() {
        return false;
    }

    @Override
    public boolean canLaunch2DIntents() {
        return true;
    }

    @Override
    public boolean onBackPressed() {
        return false;
    }

    @Override
    public boolean enterVrIfNecessary() {
        return false;
    }

    @Override
    public void maybeRegisterVrEntryHook(final ChromeActivity activity) {}

    @Override
    public void maybeUnregisterVrEntryHook() {}

    @Override
    public void onMultiWindowModeChanged(boolean isInMultiWindowMode) {}

    @Override
    public void requestToExitVrForSearchEnginePromoDialog(
            OnExitVrRequestListener listener, Activity activity) {
        listener.onSucceeded();
    }

    @Override
    public void requestToExitVr(OnExitVrRequestListener listener) {
        listener.onSucceeded();
    }

    @Override
    public void requestToExitVr(OnExitVrRequestListener listener, @UiUnsupportedMode int reason) {
        listener.onSucceeded();
    }

    @Override
    public void requestToExitVrAndRunOnSuccess(Runnable onSuccess) {
        onSuccess.run();
    }

    @Override
    public void requestToExitVrAndRunOnSuccess(Runnable onSuccess, @UiUnsupportedMode int reason) {
        onSuccess.run();
    }

    @Override
    public void onActivityShown(ChromeActivity activity) {}

    @Override
    public void onActivityHidden(ChromeActivity activity) {}

    @Override
    public boolean onDensityChanged(int oldDpi, int newDpi) {
        return false;
    }

    @Override
    public void rawTopContentOffsetChanged(float topContentOffset) {}

    @Override
    public void onNewIntentWithNative(ChromeActivity activity, Intent intent) {}

    @Override
    public void maybeHandleVrIntentPreNative(ChromeActivity activity, Intent intent) {
        if (!VrModuleProvider.getIntentDelegate().isLaunchingIntoVr(activity, intent)) return;
        if (bootsToVr() && relaunchOnMainDisplayIfNecessary(activity, intent)) return;

        if (DEBUG_LOGS) Log.i(TAG, "maybeHandleVrIntentPreNative: preparing for transition");

        // We add a black overlay view so that we can show black while the VR UI is loading. See
        // more details in {VrShellDelegate#maybeHandleVrIntentPreNative}.
        addBlackOverlayViewForActivity(activity);
        setSystemUiVisibilityForVr(activity);

        // Flag whether enter VR flow is handled already.
        AtomicBoolean enterVrHandled = new AtomicBoolean(false);

        VrModuleProvider.installModule((success) -> {
            if (enterVrHandled.getAndSet(true)) return;
            onVrModuleInstallFinished(success);
        });

        PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT, () -> {
            if (enterVrHandled.getAndSet(true)) return;
            assert !VrModule.isInstalled();
            onVrModuleInstallFailure(activity);
        }, WAITING_FOR_MODULE_TIMEOUT_MS);
    }

    @Override
    public void setVrModeEnabled(Activity activity, boolean enabled) {}

    @Override
    public void doPreInflationStartup(ChromeActivity activity, Bundle savedInstanceState) {
        if (!VrModuleProvider.getIntentDelegate().isLaunchingIntoVr(
                    activity, activity.getIntent())) {
            return;
        }

        if (bootsToVr() && !setVrMode(activity, true)) {
            activity.finish();
            return;
        }
    }

    @Override
    public boolean isDaydreamReadyDevice() {
        return ContextUtils.getApplicationContext().getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_VR_MODE_HIGH_PERFORMANCE);
    }

    @Override
    public boolean isDaydreamCurrentViewer() {
        return false;
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {}

    @Override
    protected boolean expectedDensityChange() {
        return false;
    }

    @Override
    public void initAfterModuleInstall() {
        assert false;
    }

    private void onVrModuleInstallFinished(boolean success) {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (!(activity instanceof ChromeActivity)) return;

        if (!success) {
            onVrModuleInstallFailure(activity);
            return;
        }
        assert VrModule.isInstalled();

        ENTER_VR_BROWSER_WITHOUT_FEATURE_MODULE_METRIC.record(true);

        // We need native to enter VR. Enter VR flow will automatically continue once native is
        // loaded.
        if (!LibraryLoader.getInstance().isInitialized()) return;

        boolean shouldEnterVr =
                ApplicationStatus.getStateForActivity(activity) == ActivityState.RESUMED;
        if (shouldEnterVr) {
            // Invoke the delegate with actual VR implementation.
            VrModuleProvider.getDelegate().enterVrIfNecessary();
        }
    }

    private void onVrModuleInstallFailure(Activity activity) {
        ENTER_VR_BROWSER_WITHOUT_FEATURE_MODULE_METRIC.record(false);

        // For SVR close Chrome. For standalones launch into 2D-in-VR (if that fails, close Chrome).
        if (bootsToVr()) {
            if (!setVrMode(activity, false)) {
                activity.finish();
                return;
            }
            // Set up 2D-in-VR.
            removeBlackOverlayView(activity, false);
            Toast.makeText(ContextUtils.getApplicationContext(),
                         R.string.vr_preparing_vr_toast_standalone_text, Toast.LENGTH_SHORT)
                    .show();
        } else {
            // Create immersive notification to inform user that Chrome's VR browser cannot be
            // accessed yet.
            VrFallbackUtils.showFailureNotification(activity);
            activity.finish();
        }
    }

    private boolean setVrMode(Activity activity, boolean enabled) {
        try {
            ApiHelperForN.setVrModeEnabled(activity, enabled,
                    new ComponentName(DEFAULT_VR_MODE_PACKAGE, DEFAULT_VR_MODE_CLASS));
            return true;
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Cannot unset VR mode", e);
        }
        return false;
    }
}
