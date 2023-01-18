// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;

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
        return VrCoreInstallUtils.onActivityResultWithNative(requestCode, resultCode);
    }

    @Override
    public void onNativeLibraryAvailable() {
        VrShellDelegate.onNativeLibraryAvailable();
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
    public void onMultiWindowModeChanged(boolean isInMultiWindowMode) {
        VrShellDelegate.onMultiWindowModeChanged(isInMultiWindowMode);
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
    public void setVrModeEnabled(Activity activity, boolean enabled) {
        VrShellDelegate.setVrModeEnabled(activity, enabled);
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
    public void initAfterModuleInstall() {
        VrShellDelegate.initAfterModuleInstall();
    }
}
