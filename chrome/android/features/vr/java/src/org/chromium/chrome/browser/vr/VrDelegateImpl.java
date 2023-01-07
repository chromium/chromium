// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.supplier.ObservableSupplier;

/**
 * {@link VrDelegate} implementation if the VR module is available. Mostly forwards calls to {@link
 * VrShellDelegate}.
 */
/* package */ class VrDelegateImpl extends VrDelegate {
    @Override
    public void forceExitVrImmediately() {
        VrShellDelegate.forceExitVrImmediately();
    }

    @Override
    public boolean onActivityResultWithNative(int requestCode, int resultCode) {
        return VrShellDelegate.onActivityResultWithNative(requestCode, resultCode)
                || VrCoreInstallUtils.onActivityResultWithNative(requestCode, resultCode);
    }

    @Override
    public void onNativeLibraryAvailable() {
        VrShellDelegate.onNativeLibraryAvailable();
    }

    @Override
    public boolean isInVr() {
        return VrShellDelegate.isInVr();
    }

    @Override
    public boolean canLaunch2DIntents() {
        return VrShellDelegate.canLaunch2DIntents();
    }

    @Override
    public boolean onBackPressed() {
        return VrShellDelegate.onBackPressed();
    }

    @Override
    public void handleBackPress() {
        onBackPressed();
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return VrShellDelegate.getVrModeEnabledSupplier();
    }

    @Override
    public boolean enterVrIfNecessary() {
        return VrShellDelegate.enterVrIfNecessary();
    }

    @Override
    public void maybeRegisterVrEntryHook(final Activity activity) {
        VrShellDelegate.maybeRegisterVrEntryHook(activity);
    }

    @Override
    public void maybeUnregisterVrEntryHook() {
        VrShellDelegate.maybeUnregisterVrEntryHook();
    }

    @Override
    public void onMultiWindowModeChanged(boolean isInMultiWindowMode) {
        VrShellDelegate.onMultiWindowModeChanged(isInMultiWindowMode);
    }

    @Override
    public void requestToExitVrForSearchEnginePromoDialog(
            OnExitVrRequestListener listener, Activity activity) {
        VrShellDelegate.requestToExitVrForSearchEnginePromoDialog(listener, activity);
    }

    @Override
    public void requestToExitVr(OnExitVrRequestListener listener) {
        VrShellDelegate.requestToExitVr(listener);
    }

    @Override
    public void requestToExitVr(OnExitVrRequestListener listener, @UiUnsupportedMode int reason) {
        VrShellDelegate.requestToExitVr(listener, reason);
    }

    @Override
    public void requestToExitVrAndRunOnSuccess(Runnable onSuccess) {
        VrShellDelegate.requestToExitVrAndRunOnSuccess(onSuccess);
    }

    @Override
    public void requestToExitVrAndRunOnSuccess(Runnable onSuccess, @UiUnsupportedMode int reason) {
        VrShellDelegate.requestToExitVrAndRunOnSuccess(onSuccess, reason);
    }

    @Override
    public void onActivityShown(Activity activity) {
        VrShellDelegate.onActivityShown(activity);
    }

    @Override
    public void onActivityHidden(Activity activity) {
        VrShellDelegate.onActivityHidden(activity);
    }

    @Override
    public boolean onDensityChanged(int oldDpi, int newDpi) {
        return VrShellDelegate.onDensityChanged(oldDpi, newDpi);
    }

    @Override
    public void rawTopContentOffsetChanged(float topContentOffset) {
        VrShellDelegate.rawTopContentOffsetChanged(topContentOffset);
    }

    @Override
    public void onNewIntentWithNative(Activity activity, Intent intent) {
        VrShellDelegate.onNewIntentWithNative(activity, intent);
    }

    @Override
    public void maybeHandleVrIntentPreNative(Activity activity, Intent intent) {
        VrShellDelegate.maybeHandleVrIntentPreNative(activity, intent);
    }

    @Override
    public void setVrModeEnabled(Activity activity, boolean enabled) {
        VrShellDelegate.setVrModeEnabled(activity, enabled);
    }

    @Override
    public void doPreInflationStartup(Activity activity, Bundle savedInstanceState) {
        VrShellDelegate.doPreInflationStartup(activity, savedInstanceState);
    }

    @Override
    public boolean isDaydreamReadyDevice() {
        return VrCoreInstallUtils.isDaydreamReadyDevice();
    }

    @Override
    public boolean isDaydreamCurrentViewer() {
        return VrShellDelegate.isDaydreamCurrentViewer();
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        VrShellDelegate.onSaveInstanceState(outState);
    }

    @Override
    protected boolean expectedDensityChange() {
        return VrShellDelegate.expectedDensityChange();
    }

    @Override
    public void initAfterModuleInstall() {
        VrShellDelegate.initAfterModuleInstall();
    }
}
