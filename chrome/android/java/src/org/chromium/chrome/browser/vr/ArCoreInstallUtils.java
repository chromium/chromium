// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.infobar.SimpleConfirmInfoBarBuilder;
import org.chromium.chrome.browser.modules.ModuleInstallUi;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.module_installer.engine.EngineFactory;
import org.chromium.components.module_installer.engine.InstallEngine;

/**
 * Installs AR DFM and ArCore runtimes.
 */
@JNINamespace("vr")
public class ArCoreInstallUtils implements ModuleInstallUi.FailureUiListener {
    private static final String TAG = "ArCoreInstallUtils";

    private long mNativeArCoreInstallUtils;

    private Tab mTab;

    // Instance that requested installation of ARCore.
    // Should be non-null only if there is a pending request to install ARCore.
    private static ArCoreInstallUtils sRequestInstallInstance;

    // Cached ArCoreShim instance - valid only after AR module was installed and
    // getArCoreShimInstance() was called.
    private static ArCoreShim sArCoreInstance;

    private static ArCoreShim getArCoreShimInstance() {
        if (sArCoreInstance != null) return sArCoreInstance;

        try {
            sArCoreInstance =
                    (ArCoreShim) Class.forName("org.chromium.chrome.browser.vr.ArCoreShimImpl")
                            .newInstance();
        } catch (ClassNotFoundException e) {
            // shouldn't happen - we should only call this method once AR module is installed.
            throw new RuntimeException(e);
        } catch (InstantiationException e) {
            throw new RuntimeException(e);
        } catch (IllegalAccessException e) {
            throw new RuntimeException(e);
        }

        return sArCoreInstance;
    }

    @CalledByNative
    private static ArCoreInstallUtils create(long nativeArCoreInstallUtils) {
        return new ArCoreInstallUtils(nativeArCoreInstallUtils);
    }

    private ArCoreInstallUtils(long nativeArCoreInstallUtils) {
        mNativeArCoreInstallUtils = nativeArCoreInstallUtils;
    }

    @Override
    public void onFailureUiResponse(boolean retry) {
        if (mNativeArCoreInstallUtils == 0) return;
        if (retry) {
            requestInstallArModule(mTab);
        } else {
            ArCoreInstallUtilsJni.get().onRequestInstallArModuleResult(
                    mNativeArCoreInstallUtils, false);
        }
    }

    @CalledByNative
    private boolean canRequestInstallArModule() {
        // We can only try to install the AR module if we are in a bundle mode.
        return BundleUtils.isBundle();
    }

    @CalledByNative
    private boolean shouldRequestInstallArModule() {
        try {
            // Try to find class in AR module that has not been obfuscated.
            Class.forName("com.google.ar.core.ArCoreApk");
            return false;
        } catch (ClassNotFoundException e) {
            return true;
        }
    }

    @CalledByNative
    private void requestInstallArModule(Tab tab) {
        mTab = tab;

        ModuleInstallUi ui = new ModuleInstallUi(mTab, R.string.ar_module_title, this);
        InstallEngine installEngine = new EngineFactory().getEngine();

        ui.showInstallStartUi();

        installEngine.install("ar", success -> {
            assert shouldRequestInstallArModule() != success;

            if (success) {
                // As per documentation, it's recommended to issue a call to
                // ArCoreApk.checkAvailability() early in application lifecycle & ignore the result
                // so that subsequent calls can return cached result:
                // https://developers.google.com/ar/develop/java/enable-arcore
                // This is as early in the app lifecycle as it gets for us - just after installing
                // AR module.
                getArCoreInstallStatus();
            }

            if (mNativeArCoreInstallUtils != 0) {
                if (success) {
                    ui.showInstallSuccessUi();
                    ArCoreInstallUtilsJni.get().onRequestInstallArModuleResult(
                            mNativeArCoreInstallUtils, success);
                } else {
                    ui.showInstallFailureUi();
                    // early exit - user will be offered a choice to retry & install flow will
                    // continue from onFailureUiResponse().
                    return;
                }
            }
        });
    }

    private @ArCoreShim.Availability int getArCoreInstallStatus() {
        return getArCoreShimInstance().checkAvailability(ContextUtils.getApplicationContext());
    }

    @CalledByNative
    private boolean shouldRequestInstallSupportedArCore() {
        @ArCoreShim.Availability
        int availability = getArCoreInstallStatus();
        // Skip ARCore installation if we are certain that it is already installed.
        // In all other cases, we might as well try to install it and handle installation failures.
        return availability != ArCoreShim.Availability.SUPPORTED_INSTALLED;
    }

    @CalledByNative
    private void requestInstallSupportedArCore(final Tab tab) {
        assert shouldRequestInstallSupportedArCore();

        @ArCoreShim.Availability
        int arCoreAvailability = getArCoreInstallStatus();
        final Activity activity = tab.getActivity();
        String infobarText = null;
        String buttonText = null;
        switch (arCoreAvailability) {
            case ArCoreShim.Availability.UNSUPPORTED_DEVICE_NOT_CAPABLE:
                maybeNotifyNativeOnRequestInstallSupportedArCoreResult(false);
                break;
            case ArCoreShim.Availability.UNKNOWN_CHECKING:
            case ArCoreShim.Availability.UNKNOWN_ERROR:
            case ArCoreShim.Availability.UNKNOWN_TIMED_OUT:
            case ArCoreShim.Availability.SUPPORTED_NOT_INSTALLED:
                infobarText = activity.getString(R.string.ar_core_check_infobar_install_text);
                buttonText = activity.getString(R.string.app_banner_install);
                break;
            case ArCoreShim.Availability.SUPPORTED_APK_TOO_OLD:
                infobarText = activity.getString(R.string.ar_core_check_infobar_update_text);
                buttonText = activity.getString(R.string.update_from_market);
                break;
            case ArCoreShim.Availability.SUPPORTED_INSTALLED:
                assert false;
                break;
        }

        SimpleConfirmInfoBarBuilder.Listener listener = new SimpleConfirmInfoBarBuilder.Listener() {
            @Override
            public void onInfoBarDismissed() {
                maybeNotifyNativeOnRequestInstallSupportedArCoreResult(
                        !shouldRequestInstallSupportedArCore());
            }

            @Override
            public boolean onInfoBarButtonClicked(boolean isPrimary) {
                try {
                    assert sRequestInstallInstance == null;
                    @ArCoreShim.InstallStatus
                    int installStatus = getArCoreShimInstance().requestInstall(activity, true);

                    if (installStatus == ArCoreShim.InstallStatus.INSTALL_REQUESTED) {
                        // Install flow will resume in onArCoreRequestInstallReturned, mark that
                        // there is active request. Native code notification will be deferred until
                        // our activity gets resumed.
                        sRequestInstallInstance = ArCoreInstallUtils.this;
                    } else if (installStatus == ArCoreShim.InstallStatus.INSTALLED) {
                        // No need to install - notify native code.
                        maybeNotifyNativeOnRequestInstallSupportedArCoreResult(true);
                    }

                } catch (ArCoreShim.UnavailableDeviceNotCompatibleException e) {
                    sRequestInstallInstance = null;
                    Log.w(TAG, "ARCore installation request failed with exception: %s",
                            e.toString());

                    maybeNotifyNativeOnRequestInstallSupportedArCoreResult(false);
                } catch (ArCoreShim.UnavailableUserDeclinedInstallationException e) {
                    maybeNotifyNativeOnRequestInstallSupportedArCoreResult(false);
                }

                return false;
            }

            @Override
            public boolean onInfoBarLinkClicked() {
                return false;
            }
        };
        // TODO(ijamardo, https://crbug.com/838833): Add icon for AR info bar.
        SimpleConfirmInfoBarBuilder.create(tab, listener, InfoBarIdentifier.AR_CORE_UPGRADE_ANDROID,
                R.drawable.ic_error_outline_googblue_24dp, infobarText, buttonText, null, null,
                true);
    }

    /**
     * Helper used to notify native code about the result of the request to install ARCore.
     */
    private void maybeNotifyNativeOnRequestInstallSupportedArCoreResult(boolean success) {
        if (mNativeArCoreInstallUtils != 0) {
            ArCoreInstallUtilsJni.get().onRequestInstallSupportedArCoreResult(
                    mNativeArCoreInstallUtils, success);
        }
    }

    private void onArCoreRequestInstallReturned(Activity activity) {
        try {
            // Since |userRequestedInstall| parameter is false, the below call should
            // throw if ARCore is still not installed - no need to check the result.
            getArCoreShimInstance().requestInstall(activity, false);
            maybeNotifyNativeOnRequestInstallSupportedArCoreResult(true);
        } catch (ArCoreShim.UnavailableDeviceNotCompatibleException e) {
            Log.w(TAG, "Exception thrown when trying to validate install state of ARCore: %s",
                    e.toString());
            maybeNotifyNativeOnRequestInstallSupportedArCoreResult(false);
        } catch (ArCoreShim.UnavailableUserDeclinedInstallationException e) {
            maybeNotifyNativeOnRequestInstallSupportedArCoreResult(false);
        }
    }

    /**
     * This method should be called by the Activity that gets resumed.
     * We are only interested in the cases where our current Activity got paused
     * as a result of a call to ArCoreApk.requestInstall() method.
     */
    public static void onResumeActivityWithNative(Activity activity) {
        if (sRequestInstallInstance != null) {
            sRequestInstallInstance.onArCoreRequestInstallReturned(activity);
            sRequestInstallInstance = null;
        }
    }

    public static void installArCoreDeviceProviderFactory() {
        ArCoreInstallUtilsJni.get().installArCoreDeviceProviderFactory();
    }

    @NativeMethods
    /* package */ interface ArConsentPromptNative {
        void onRequestInstallArModuleResult(long nativeArCoreConsentPrompt, boolean success);
        void onRequestInstallSupportedArCoreResult(long nativeArCoreConsentPrompt, boolean success);
        void installArCoreDeviceProviderFactory();
    }
}
