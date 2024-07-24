// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.annotation.SuppressLint;
import android.app.Service;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;
import android.os.Process;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.SafeModeController;
import org.chromium.android_webview.common.services.ISafeModeService;
import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;

import javax.annotation.concurrent.GuardedBy;

/**
 * A Service to manage WebView SafeMode state. This Service exposes an interface by which trusted
 * services (as determined by a hardcoded allowlist) can enable or disable WebView SafeMode. This
 * Service is then responsible for propagating this information to embedded WebView implementations
 * as they start up.
 */
public final class SafeModeService extends Service {
    private static final String TAG = "WebViewSafeMode";
    @VisibleForTesting public static final String SAFEMODE_ACTIONS_KEY = "SAFEMODE_ACTIONS";

    private static final Object sLock = new Object();

    /**
     * Helper class for statically defining a trusted package's identity and verifying this at
     * runtime.
     */
    @VisibleForTesting
    public static class TrustedPackage {
        private String mPackageName;
        private byte[] mReleaseCertHash;
        private byte[] mDebugCertHash;

        /**
         * Represents the identity of a package trusted by this service.
         *
         * @param packageName The package name of the trusted caller.
         * @param releaseCertHash The SHA256 hash of the caller's <b>release</b> (production)
         *     certificate. This is honored on any type of Android build. This value is required. If
         *     the trusted caller
         * @param debugCertHash This is similar to {@code releaseCertHash}, but for the <b>debug</b>
         *         (development)
         *     certificate. This is honored on userdebug/eng Android images but not on user Android
         *     builds. If the caller always uses the same signing certificate, this parameter should
         *     be {@code null} and the certificate hash should be passed into {@code
         *     releaseCertHash} instead.
         */
        public TrustedPackage(
                @NonNull String packageName,
                @NonNull byte[] releaseCertHash,
                @Nullable byte[] debugCertHash) {
            mPackageName = packageName;
            mReleaseCertHash = releaseCertHash;
            mDebugCertHash = debugCertHash;
        }

        // Whether or not this is a debug build. This can be mocked in tests.
        protected boolean isDebugAndroid() {
            return BuildInfo.isDebugAndroid();
        }

        public boolean verify(String packageName) {
            if (!mPackageName.equals(packageName)) return false;

            return hasSigningCertificate(packageName, mReleaseCertHash)
                    || (isDebugAndroid() && hasSigningCertificate(packageName, mDebugCertHash));
        }

        @SuppressLint("PackageManagerGetSignatures")
        // https://stackoverflow.com/questions/39192844/android-studio-warning-when-using-packagemanager-get-signatures
        private static boolean hasSigningCertificate(
                @NonNull String packageName, @Nullable byte[] expectedCertHash) {
            if (expectedCertHash == null) {
                return false;
            }
            final Context context = ContextUtils.getApplicationContext();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                PackageManager pm = context.getPackageManager();
                return pm.hasSigningCertificate(
                        packageName, expectedCertHash, PackageManager.CERT_INPUT_SHA256);
            }
            PackageInfo info =
                    PackageUtils.getPackageInfo(packageName, PackageManager.GET_SIGNATURES);
            if (info != null) {
                Signature[] signatures = info.signatures;
                if (signatures == null) {
                    return false;
                }
                for (Signature signature : signatures) {
                    if (Arrays.equals(expectedCertHash, sha256Hash(signature))) {
                        return true;
                    }
                }
            }
            return false; // no matches
        }

        @Nullable
        private static byte[] sha256Hash(@Nullable Signature signature) {
            if (signature == null) return null;
            try {
                return MessageDigest.getInstance("SHA256").digest(signature.toByteArray());
            } catch (NoSuchAlgorithmException e) {
                // This shouldn't happen.
                return null;
            }
        }
    }

    private static final TrustedPackage[] sTrustedPackages = {
        new TrustedPackage(
                "com.android.vending",
                new byte[] {
                    (byte) 0xf0,
                    (byte) 0xfd,
                    (byte) 0x6c,
                    (byte) 0x5b,
                    (byte) 0x41,
                    (byte) 0x0f,
                    (byte) 0x25,
                    (byte) 0xcb,
                    (byte) 0x25,
                    (byte) 0xc3,
                    (byte) 0xb5,
                    (byte) 0x33,
                    (byte) 0x46,
                    (byte) 0xc8,
                    (byte) 0x97,
                    (byte) 0x2f,
                    (byte) 0xae,
                    (byte) 0x30,
                    (byte) 0xf8,
                    (byte) 0xee,
                    (byte) 0x74,
                    (byte) 0x11,
                    (byte) 0xdf,
                    (byte) 0x91,
                    (byte) 0x04,
                    (byte) 0x80,
                    (byte) 0xad,
                    (byte) 0x6b,
                    (byte) 0x2d,
                    (byte) 0x60,
                    (byte) 0xdb,
                    (byte) 0x83
                },
                new byte[] {
                    (byte) 0x19,
                    (byte) 0x75,
                    (byte) 0xb2,
                    (byte) 0xf1,
                    (byte) 0x71,
                    (byte) 0x77,
                    (byte) 0xbc,
                    (byte) 0x89,
                    (byte) 0xa5,
                    (byte) 0xdf,
                    (byte) 0xf3,
                    (byte) 0x1f,
                    (byte) 0x9e,
                    (byte) 0x64,
                    (byte) 0xa6,
                    (byte) 0xca,
                    (byte) 0xe2,
                    (byte) 0x81,
                    (byte) 0xa5,
                    (byte) 0x3d,
                    (byte) 0xc1,
                    (byte) 0xd1,
                    (byte) 0xd5,
                    (byte) 0x9b,
                    (byte) 0x1d,
                    (byte) 0x14,
                    (byte) 0x7f,
                    (byte) 0xe1,
                    (byte) 0xc8,
                    (byte) 0x2a,
                    (byte) 0xfa,
                    (byte) 0x00
                }),
    };

    // Auto-disable SafeMode after 30 days.
    @VisibleForTesting
    public static final long SAFE_MODE_ENABLED_TIME_LIMIT_MS = TimeUnit.DAYS.toMillis(30);

    /**
     * A mockable clock. Returns the current time in ms since the unix epoch. For reference, the
     * default implementation is {@code System.currentTimeMillis()}.
     */
    @VisibleForTesting
    public interface Clock {
        long currentTimeMillis();
    }

    @GuardedBy("sLock")
    private static Clock sClock = System::currentTimeMillis;

    private static final String SHARED_PREFS_FILE = "webview_safemode_prefs";

    @VisibleForTesting public static final String LAST_MODIFIED_TIME_KEY = "LAST_MODIFIED_TIME";

    private boolean isCallerTrusted() {
        final Context context = ContextUtils.getApplicationContext();
        PackageManager pm = context.getPackageManager();
        String[] packagesInUid = pm.getPackagesForUid(Binder.getCallingUid());

        if (packagesInUid == null) {
            Log.e(
                    TAG,
                    "Unable to find any packages associated with calling UID ("
                            + Binder.getCallingUid()
                            + ")");
            return false;
        }

        if (Binder.getCallingUid() == Process.myUid()) {
            // Trust the nonembedded WebView provider UID. We don't currently expect the WebView
            // provider itself to enable SafeMode in production (although we may consider this in
            // the future). Right now, this is permitted for testing purposes.
            return true;
        }

        // We trust the caller if any package name in the UID matches any of the TrustedPackages in
        // our allowlist, since all packages in the same UID must be signed by the same certificate
        // set. In practice, we only expect a single package per UID because `android:sharedUserId`
        // is deprecated.
        for (String packageName : packagesInUid) {
            for (TrustedPackage trustedPackage : sTrustedPackages) {
                if (trustedPackage.verify(packageName)) {
                    return true;
                }
            }
        }

        return false;
    }

    private final ISafeModeService.Stub mBinder =
            new ISafeModeService.Stub() {
                @Override
                public void setSafeMode(List<String> actions) {
                    if (!isCallerTrusted()) {
                        throw new SecurityException(
                                "setSafeMode() may only be called by a trusted app");
                    }

                    SafeModeService.setSafeMode(actions);
                }

                // This used by the Dev UI SafeMode Fragment to display the activation time.
                @Override
                public long getSafeModeActivationTimestamp() {
                    return getLastModifiedTime();
                }
            };

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    private static SharedPreferences getSharedPreferences() {
        final Context context = ContextUtils.getApplicationContext();
        return context.getSharedPreferences(SHARED_PREFS_FILE, Context.MODE_PRIVATE);
    }

    /**
     * Sets the SafeMode config. This includes persisting the set of actions, toggling component
     * state, etc.
     *
     * <p>This may only be called from the same process SafeModeService is declared to run in via
     * the "android:process" attribute. Callers from other processes must bind to the Service via
     * the AIDL interface.
     */
    public static void setSafeMode(List<String> actions) {
        synchronized (sLock) {
            SafeModeService.setSafeModeLocked(actions);
        }
    }

    @GuardedBy("sLock")
    private static void setSafeModeLocked(List<String> actions) {
        boolean enableSafeMode = actions != null && !actions.isEmpty();

        Set<String> oldActions = new HashSet<>();
        oldActions.addAll(
                getSharedPreferences().getStringSet(SAFEMODE_ACTIONS_KEY, Collections.emptySet()));
        Set<String> actionsToPersist = new HashSet<>(actions);
        SharedPreferences.Editor editor = getSharedPreferences().edit();
        if (enableSafeMode) {
            long currentTime = sClock.currentTimeMillis();
            editor.putLong(LAST_MODIFIED_TIME_KEY, currentTime);

            editor.putStringSet(SAFEMODE_ACTIONS_KEY, actionsToPersist);
        } else {
            editor.clear();
        }

        // Ignore errors, since there's no way to recover. Commit changes async to avoid
        // blocking the service.
        editor.apply();

        final Context context = ContextUtils.getApplicationContext();
        ComponentName safeModeComponent =
                new ComponentName(context, SafeModeController.SAFE_MODE_STATE_COMPONENT);

        int newState =
                enableSafeMode
                        ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                        : PackageManager.COMPONENT_ENABLED_STATE_DEFAULT;
        context.getPackageManager()
                .setComponentEnabledSetting(
                        safeModeComponent, newState, PackageManager.DONT_KILL_APP);
        if (SafeModeController.getInstance().getRegisteredActions() != null) {
            NonEmbeddedSafeModeActionsSetupCleanup.executeNonEmbeddedActionsOnStateChange(
                    oldActions, actionsToPersist);
        }
    }

    @GuardedBy("sLock")
    private static void disableSafeMode() {
        setSafeModeLocked(Arrays.asList());
    }

    @GuardedBy("sLock")
    private static boolean shouldAutoDisableSafeMode() {
        long lastModifiedTime = getSharedPreferences().getLong(LAST_MODIFIED_TIME_KEY, 0L);
        long currentTime = sClock.currentTimeMillis();
        long timeSinceLastSafeModeConfig = currentTime - lastModifiedTime;

        // It shouldn't be possible for lastModifiedTime to happen in the future (greater than
        // currentTime). The user may have changed the clock on their device. Treat the config as
        // expired in this case because we don't want to be in SafeMode arbitrarily long.
        if (timeSinceLastSafeModeConfig < 0) {
            Log.w(
                    TAG,
                    "Config timestamp is (%d) but current time is (%d); disabling SafeMode",
                    lastModifiedTime,
                    currentTime);
            return true;
        }

        return timeSinceLastSafeModeConfig >= SAFE_MODE_ENABLED_TIME_LIMIT_MS;
    }

    public static void setClockForTesting(Clock clock) {
        synchronized (sLock) {
            sClock = clock;
        }
    }

    @NonNull
    public static Set<String> getSafeModeConfig() {
        synchronized (sLock) {
            final Context context = ContextUtils.getApplicationContext();
            if (!SafeModeController.getInstance().isSafeModeEnabled(context.getPackageName())) {
                return new HashSet<>();
            }
            if (shouldAutoDisableSafeMode()) {
                disableSafeMode();
                return new HashSet<>();
            }

            // Returning an empty Set in the absence of persisted actions ensures the caller
            // doesn't crash when iterating over the return value.
            Set<String> actions =
                    getSharedPreferences()
                            .getStringSet(SAFEMODE_ACTIONS_KEY, Collections.emptySet());
            if (actions.isEmpty()) {
                Log.w(TAG, "Config is empty even though SafeMode is enabled; disabling SafeMode");
                disableSafeMode();
            }
            return actions;
        }
    }

    public static long getLastModifiedTime() {
        return getSharedPreferences().getLong(LAST_MODIFIED_TIME_KEY, 0L);
    }

    public static void clearSharedPrefsForTesting() {
        synchronized (sLock) {
            getSharedPreferences().edit().clear().apply();
        }
    }

    public static void removeSharedPrefKeyForTesting(String key) {
        synchronized (sLock) {
            getSharedPreferences().edit().remove(key).apply();
        }
    }
}
