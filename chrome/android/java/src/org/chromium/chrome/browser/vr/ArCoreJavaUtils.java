// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.IntDef;

import dalvik.system.BaseDexClassLoader;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.infobar.SimpleConfirmInfoBarBuilder;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.module_installer.ModuleInstaller;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Provides ARCore classes access to java-related app functionality.
 */
@JNINamespace("vr")
public class ArCoreJavaUtils {
    private static final int MIN_SDK_VERSION = Build.VERSION_CODES.O;
    private static final String AR_CORE_PACKAGE = "com.google.ar.core";
    private static final String METADATA_KEY_MIN_APK_VERSION = "com.google.ar.core.min_apk_version";
    private static final int ARCORE_NOT_INSTALLED_VERSION_CODE = -1;
    private static final String AR_CORE_MARKET_URI = "market://details?id=" + AR_CORE_PACKAGE;

    // TODO(https://crbug.com/850840): Listen to ActivityOnResult to know when
    // the intent to install ARCore returns.
    private static final int AR_CORE_INSTALL_RESULT = -1;

    // This purely exists to silence lint errors.
    // TODO(crbug.com/884321): Find a better solution.
    private static final int[] SILENCE_LINT_ERRORS = new int[] {R.string.ar_module_title};

    @IntDef({ArCoreInstallStatus.ARCORE_NEEDS_UPDATE, ArCoreInstallStatus.ARCORE_NOT_INSTALLED,
            ArCoreInstallStatus.ARCORE_INSTALLED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ArCoreInstallStatus {
        int ARCORE_NEEDS_UPDATE = 0;
        int ARCORE_NOT_INSTALLED = 1;
        int ARCORE_INSTALLED = 2;
    }

    private long mNativeArCoreJavaUtils;
    private boolean mAppInfoInitialized;
    private int mAppMinArCoreApkVersionCode = ARCORE_NOT_INSTALLED_VERSION_CODE;

    /**
     * Gets the current application context.
     *
     * @return Context The application context.
     */
    @CalledByNative
    private static Context getApplicationContext() {
        return ContextUtils.getApplicationContext();
    }

    /**
     * Determines whether ARCore's SDK should be loaded. Currently, this only
     * depends on the OS version, but could be more sophisticated.
     *
     * @return true if the SDK should be loaded.
     */
    @CalledByNative
    private static boolean shouldLoadArCoreSdk() {
        return Build.VERSION.SDK_INT >= MIN_SDK_VERSION;
    }

    @CalledByNative
    private static ArCoreJavaUtils create(long nativeArCoreJavaUtils) {
        ThreadUtils.assertOnUiThread();
        return new ArCoreJavaUtils(nativeArCoreJavaUtils);
    }

    @CalledByNative
    private static String getArCoreShimLibraryPath() {
        return ((BaseDexClassLoader) ContextUtils.getApplicationContext().getClassLoader())
                .findLibrary("arcore_sdk_c_minimal");
    }

    private ArCoreJavaUtils(long nativeArCoreJavaUtils) {
        mNativeArCoreJavaUtils = nativeArCoreJavaUtils;
        initializeAppInfo();
    }

    @CalledByNative
    private void onNativeDestroy() {
        mNativeArCoreJavaUtils = 0;
    }

    private void initializeAppInfo() {
        Context context = getApplicationContext();
        PackageManager packageManager = context.getPackageManager();
        String packageName = context.getPackageName();

        Bundle metadata;
        try {
            metadata = packageManager.getApplicationInfo(packageName, PackageManager.GET_META_DATA)
                               .metaData;
        } catch (PackageManager.NameNotFoundException e) {
            throw new IllegalStateException("Could not load application package metadata", e);
        }

        if (metadata.containsKey(METADATA_KEY_MIN_APK_VERSION)) {
            mAppMinArCoreApkVersionCode = metadata.getInt(METADATA_KEY_MIN_APK_VERSION);
        } else {
            throw new IllegalStateException(
                    "Application manifest must contain meta-data " + METADATA_KEY_MIN_APK_VERSION);
        }
        mAppInfoInitialized = true;

        // Need to be called before trying to access the AR module.
        ModuleInstaller.init();
    }

    private int getArCoreApkVersionNumber() {
        try {
            PackageInfo info = getApplicationContext().getPackageManager().getPackageInfo(
                    AR_CORE_PACKAGE, PackageManager.GET_SERVICES);
            int version = info.versionCode;
            if (version == 0) {
                return ARCORE_NOT_INSTALLED_VERSION_CODE;
            }
            return version;
        } catch (PackageManager.NameNotFoundException e) {
            return ARCORE_NOT_INSTALLED_VERSION_CODE;
        }
    }

    private @ArCoreInstallStatus int getArCoreInstallStatus() {
        assert mAppInfoInitialized;
        int arCoreApkVersionNumber = getArCoreApkVersionNumber();
        if (arCoreApkVersionNumber == ARCORE_NOT_INSTALLED_VERSION_CODE) {
            return ArCoreInstallStatus.ARCORE_NOT_INSTALLED;
        } else if (arCoreApkVersionNumber == 0
                || arCoreApkVersionNumber < mAppMinArCoreApkVersionCode) {
            return ArCoreInstallStatus.ARCORE_NEEDS_UPDATE;
        }
        return ArCoreInstallStatus.ARCORE_INSTALLED;
    }

    @CalledByNative
    private boolean shouldRequestInstallSupportedArCore() {
        return getArCoreInstallStatus() != ArCoreInstallStatus.ARCORE_INSTALLED;
    }

    @CalledByNative
    private void requestInstallArModule() {
        // TODO(crbug.com/863064): This is a placeholder UI. Replace once proper UI is spec'd.
        Toast.makeText(ContextUtils.getApplicationContext(), R.string.ar_module_install_start_text,
                     Toast.LENGTH_SHORT)
                .show();

        ModuleInstaller.install("ar", success -> {
            // TODO(crbug.com/863064): This is a placeholder UI. Replace once proper UI is spec'd.
            int mToastTextRes = success ? R.string.ar_module_install_success_text
                                        : R.string.ar_module_install_failure_text;
            Toast.makeText(ContextUtils.getApplicationContext(), mToastTextRes, Toast.LENGTH_SHORT)
                    .show();

            if (mNativeArCoreJavaUtils != 0) {
                assert shouldRequestInstallArModule() == !success;
                nativeOnRequestInstallArModuleResult(mNativeArCoreJavaUtils, success);
            }
        });
    }

    @CalledByNative
    private void requestInstallSupportedArCore(final Tab tab) {
        assert shouldRequestInstallSupportedArCore();
        @ArCoreInstallStatus
        int arcoreInstallStatus = getArCoreInstallStatus();
        final Activity activity = tab.getActivity();
        String infobarText = null;
        String buttonText = null;
        switch (arcoreInstallStatus) {
            case ArCoreInstallStatus.ARCORE_NOT_INSTALLED:
                infobarText = activity.getString(R.string.ar_core_check_infobar_install_text);
                buttonText = activity.getString(R.string.app_banner_install);
                break;
            case ArCoreInstallStatus.ARCORE_NEEDS_UPDATE:
                infobarText = activity.getString(R.string.ar_core_check_infobar_update_text);
                buttonText = activity.getString(R.string.update_from_market);
                break;
            case ArCoreInstallStatus.ARCORE_INSTALLED:
                assert false;
                break;
        }

        SimpleConfirmInfoBarBuilder.Listener listener = new SimpleConfirmInfoBarBuilder.Listener() {
            @Override
            public void onInfoBarDismissed() {
                if (mNativeArCoreJavaUtils != 0) {
                    nativeOnRequestInstallSupportedArCoreCanceled(mNativeArCoreJavaUtils);
                }
            }

            @Override
            public boolean onInfoBarButtonClicked(boolean isPrimary) {
                activity.startActivityForResult(
                        new Intent(Intent.ACTION_VIEW, Uri.parse(AR_CORE_MARKET_URI)),
                        AR_CORE_INSTALL_RESULT);
                return false;
            }
        };
        // TODO(ijamardo, https://crbug.com/838833): Add icon for AR info bar.
        SimpleConfirmInfoBarBuilder.create(tab, listener, InfoBarIdentifier.AR_CORE_UPGRADE_ANDROID,
                R.drawable.vr_services, infobarText, buttonText, null, true);
    }

    @CalledByNative
    private boolean shouldRequestInstallArModule() {
        try {
            // Try to find class in AR module that has not been obfuscated.
            Class.forName("com.google.vr.dynamite.client.UsedByNative");
            return false;
        } catch (ClassNotFoundException e) {
            return true;
        }
    }

    private native void nativeOnRequestInstallArModuleResult(
            long nativeArCoreJavaUtils, boolean success);
    private native void nativeOnRequestInstallSupportedArCoreCanceled(long nativeArCoreJavaUtils);
}
