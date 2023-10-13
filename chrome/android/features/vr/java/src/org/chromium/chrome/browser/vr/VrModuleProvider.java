// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.BundleUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.modules.ModuleInstallUi;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.module_installer.engine.InstallListener;
import org.chromium.ui.base.WindowAndroid;

/**
 * Instantiates the VR delegates. If the VR module is not available this provider will
 * instantiate a fallback implementation.
 */
@JNINamespace("vr")
public class VrModuleProvider implements ModuleInstallUi.FailureUiListener {
    private static VrDelegateProvider sDelegateProvider;

    private long mNativeVrModuleProvider;
    private Tab mTab;

    /**
     * Need to be called after native libraries are available. Has no effect if VR is not compiled
     * into Chrome.
     */
    public static void maybeInit() {
        // Always install the VR module on Daydream-ready devices.
        maybeRequestModuleIfDaydreamReady();
    }

    /**
     * Requests deferred installation of the VR module on Daydream-ready devices. Has no effect if
     * VR is not compiled into Chrome.
     */
    public static void maybeRequestModuleIfDaydreamReady() {
        if (!BundleUtils.isBundle()) return;
        if (VrModule.isInstalled()) return;
        if (!getDelegate().isDaydreamReadyDevice()) return;

        // Installs module when on unmetered network connection and device is charging.
        VrModule.installDeferred();
    }

    public static VrDelegate getDelegate() {
        return getDelegateProvider().getDelegate();
    }

    /* package */ static void installModule(InstallListener listener) {
        VrModule.install((success) -> {
            if (success) {
                // Re-create delegate provider.
                sDelegateProvider = null;
                VrDelegate delegate = getDelegate();
                assert !(delegate instanceof VrDelegateFallback);
                delegate.initAfterModuleInstall();
            }
            listener.onComplete(success);
        });
    }

    // TODO(crbug.com/870055): JNI should be registered in the shared VR library's JNI_OnLoad
    // function. Do this once we have a shared VR library.
    public static void registerJni() {
        VrModuleProviderJni.get().registerJni();
    }

    private static VrDelegateProvider getDelegateProvider() {
        if (sDelegateProvider == null) {
            if (!VrModule.isInstalled()) {
                sDelegateProvider = new VrDelegateProviderFallback();
            } else {
                sDelegateProvider = VrModule.getImpl();
            }
        }
        return sDelegateProvider;
    }

    @CalledByNative
    private static VrModuleProvider create(long nativeVrModuleProvider) {
        return new VrModuleProvider(nativeVrModuleProvider);
    }

    @CalledByNative
    private static boolean isModuleInstalled() {
        return VrModule.isInstalled();
    }

    @Override
    public void onFailureUiResponse(boolean retry) {
        if (mNativeVrModuleProvider == 0) return;
        if (retry) {
            installModule(mTab);
        } else {
            VrModuleProviderJni.get().onInstalledModule(
                    mNativeVrModuleProvider, VrModuleProvider.this, false);
        }
    }

    private VrModuleProvider(long nativeVrModuleProvider) {
        mNativeVrModuleProvider = nativeVrModuleProvider;
    }

    @CalledByNative
    private void onNativeDestroy() {
        mNativeVrModuleProvider = 0;
    }

    @CalledByNative
    private void installModule(Tab tab) {
        mTab = tab;
        ModuleInstallUi.Delegate moduleInstallUiDelegate = new ModuleInstallUi.Delegate() {
            @Override
            public WindowAndroid getWindowAndroid() {
                return mTab.getWindowAndroid();
            }

            @Override
            public Context getContext() {
                return mTab.getWindowAndroid() != null ? mTab.getWindowAndroid().getActivity().get()
                                                       : null;
            }
        };
        ModuleInstallUi ui =
                new ModuleInstallUi(moduleInstallUiDelegate, R.string.vr_module_title, this);
        ui.showInstallStartUi();
        installModule((success) -> {
            if (mNativeVrModuleProvider != 0) {
                if (!success) {
                    ui.showInstallFailureUi();
                    return;
                }
                ui.showInstallSuccessUi();
                VrModuleProviderJni.get().onInstalledModule(
                        mNativeVrModuleProvider, VrModuleProvider.this, success);
            }
        });
    }

    @NativeMethods
    interface Natives {
        void registerJni();
        void onInstalledModule(
                long nativeVrModuleProvider, VrModuleProvider caller, boolean success);
    }
}
