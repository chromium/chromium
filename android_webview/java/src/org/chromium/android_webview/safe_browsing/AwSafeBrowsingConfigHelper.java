// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.safe_browsing;

import androidx.annotation.IntDef;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.android_webview.ManifestMetadataUtil;
import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.ScopedSysTraceEvent;

/**
 * Helper class for getting the configuration settings related to safebrowsing in WebView.
 */
@JNINamespace("android_webview")
public class AwSafeBrowsingConfigHelper {
    private static volatile boolean sSafeBrowsingUserOptIn;
    private static volatile boolean sEnabledByManifest;

    // Used to record the UMA histogram SafeBrowsing.WebView.AppOptIn. Since these values are
    // persisted to logs, they should never be renumbered or reused.
    @IntDef({AppOptIn.NO_PREFERENCE, AppOptIn.OPT_IN, AppOptIn.OPT_OUT})
    @interface AppOptIn {
        int NO_PREFERENCE = 0;
        int OPT_IN = 1;
        int OPT_OUT = 2;

        int COUNT = 3;
    }

    private static void recordAppOptIn(@AppOptIn int value) {
        RecordHistogram.recordEnumeratedHistogram(
                "SafeBrowsing.WebView.AppOptIn", value, AppOptIn.COUNT);
    }

    public static void setSafeBrowsingEnabledByManifest(boolean enabled) {
        sEnabledByManifest = enabled;
    }

    @CalledByNative
    public static boolean getSafeBrowsingEnabledByManifest() {
        return sEnabledByManifest;
    }

    // Should only be called once during startup. Calling this multiple times will skew UMA metrics.
    public static void maybeEnableSafeBrowsingFromManifest() {
        try (ScopedSysTraceEvent e = ScopedSysTraceEvent.scoped(
                     "AwSafeBrowsingConfigHelper.maybeEnableSafeBrowsingFromManifest")) {
            Boolean appOptIn = getOptInPreferenceTraced();
            if (appOptIn == null) {
                recordAppOptIn(AppOptIn.NO_PREFERENCE);
            } else if (appOptIn) {
                recordAppOptIn(AppOptIn.OPT_IN);
            } else {
                recordAppOptIn(AppOptIn.OPT_OUT);
            }

            // If the app specifies something, fallback to the app's preference, otherwise check for
            // the existence of the CLI switch.
            setSafeBrowsingEnabledByManifest(
                    appOptIn == null ? !isDisabledByCommandLine() : appOptIn);

            Callback<Boolean> cb = verifyAppsValue
                    -> setSafeBrowsingUserOptIn(Boolean.TRUE.equals(verifyAppsValue));
            PlatformServiceBridge.getInstance().querySafeBrowsingUserConsent(cb);
        }
    }

    private static Boolean getOptInPreferenceTraced() {
        try (ScopedSysTraceEvent e = ScopedSysTraceEvent.scoped(
                     "AwSafeBrowsingConfigHelper.getAppOptInPreference")) {
            return ManifestMetadataUtil.getSafeBrowsingAppOptInPreference();
        }
    }

    private static boolean isDisabledByCommandLine() {
        try (ScopedSysTraceEvent e = ScopedSysTraceEvent.scoped(
                     "AwSafeBrowsingConfigHelper.isDisabledByCommandLine")) {
            CommandLine cli = CommandLine.getInstance();
            // Disable flag has higher precedence than the default
            return cli.hasSwitch(AwSwitches.WEBVIEW_DISABLE_SAFEBROWSING_SUPPORT);
        }
    }

    // Can be called from any thread. This returns true or false, depending on user opt-in
    // preference. This returns false if we don't know yet what the user's preference is.
    @CalledByNative
    private static boolean getSafeBrowsingUserOptIn() {
        return sSafeBrowsingUserOptIn;
    }

    // This feature checks if GMS is present, enabled, accessible to WebView and has minimum
    // version to support safe browsing
    @CalledByNative
    private static boolean canUseGms() {
        return PlatformServiceBridge.getInstance().canUseGms();
    }

    public static void setSafeBrowsingUserOptIn(boolean optin) {
        sSafeBrowsingUserOptIn = optin;
    }

    // Not meant to be instantiated.
    private AwSafeBrowsingConfigHelper() {}
}
