// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;
import android.content.ComponentName;
import android.content.pm.PackageManager;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.compat.ApiHelperForN;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;

/**
 * Fallback {@link VrDelegate} implementation if the VR module is not available.
 */
/* package */ class VrDelegateFallback extends VrDelegate {
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
    public boolean onBackPressed() {
        return false;
    }

    @Override
    public void handleBackPress() {}

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return new ObservableSupplierImpl<>();
    }

    @Override
    public void onMultiWindowModeChanged(boolean isInMultiWindowMode) {}

    @Override
    public void onActivityShown(Activity activity) {}

    @Override
    public void onActivityHidden(Activity activity) {}

    @Override
    public void setVrModeEnabled(Activity activity, boolean enabled) {}

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
    public void initAfterModuleInstall() {
        assert false;
    }

    private void onVrModuleInstallFinished(boolean success) {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();

        if (!success) {
            onVrModuleInstallFailure(activity);
            return;
        }
        assert VrModule.isInstalled();

        // We need native to enter VR. Enter VR flow will automatically continue once native is
        // loaded.
        if (!LibraryLoader.getInstance().isInitialized()) return;
    }

    private void onVrModuleInstallFailure(Activity activity) {
        // Create immersive notification to inform user that Chrome's VR browser cannot be
        // accessed yet.
        VrFallbackUtils.showFailureNotification(activity);
        activity.finish();
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
