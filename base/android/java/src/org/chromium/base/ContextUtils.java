// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.Activity;
import android.app.Application;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Handler;
import android.os.Process;
import android.preference.PreferenceManager;

import androidx.annotation.Nullable;

import org.jni_zero.JNINamespace;

import org.chromium.build.BuildConfig;

/** This class provides Android application context related utility methods. */
@JNINamespace("base::android")
public class ContextUtils {
    private static final String TAG = "ContextUtils";
    private static Context sApplicationContext;

    /**
     * Flag for {@link Context#registerReceiver}: The receiver can receive broadcasts from other
     * Apps. Has the same behavior as marking a statically registered receiver with "exported=true".
     *
     * TODO(mthiesse): Move to ApiHelperForT when we build against T SDK.
     */
    public static final int RECEIVER_EXPORTED = 0x2;

    public static final int RECEIVER_NOT_EXPORTED = 0x4;

    /** Initialization-on-demand holder. This exists for thread-safe lazy initialization. */
    private static class Holder {
        // Not final for tests.
        private static SharedPreferences sSharedPreferences = fetchAppSharedPreferences();
    }

    /**
     * Get the Android application context.
     *
     * Under normal circumstances there is only one application context in a process, so it's safe
     * to treat this as a global. In WebView it's possible for more than one app using WebView to be
     * running in a single process, but this mechanism is rarely used and this is not the only
     * problem in that scenario, so we don't currently forbid using it as a global.
     *
     * Do not downcast the context returned by this method to Application (or any subclass). It may
     * not be an Application object; it may be wrapped in a ContextWrapper. The only assumption you
     * may make is that it is a Context whose lifetime is the same as the lifetime of the process.
     */
    public static Context getApplicationContext() {
        return sApplicationContext;
    }

    /**
     * Initializes the java application context.
     *
     * This should be called exactly once early on during startup, before native is loaded and
     * before any other clients make use of the application context through this class.
     *
     * @param appContext The application context.
     */
    public static void initApplicationContext(Context appContext) {
        // Conceding that occasionally in tests, native is loaded before the browser process is
        // started, in which case the browser process re-sets the application context.
        assert sApplicationContext == null
                || sApplicationContext == appContext
                || ((ContextWrapper) sApplicationContext).getBaseContext() == appContext;
        initJavaSideApplicationContext(appContext);
    }

    /**
     * Only called by the static holder class and tests.
     *
     * @return The application-wide shared preferences.
     */
    @SuppressWarnings("DefaultSharedPreferencesCheck")
    private static SharedPreferences fetchAppSharedPreferences() {
        // This may need to create the prefs directory if we've never used shared prefs before, so
        // allow disk writes. This is rare but can happen if code used early in startup reads prefs.
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            return PreferenceManager.getDefaultSharedPreferences(sApplicationContext);
        }
    }

    /**
     * This is used to ensure that we always use the application context to fetch the default shared
     * preferences. This avoids needless I/O for android N and above. It also makes it clear that
     * the app-wide shared preference is desired, rather than the potentially context-specific one.
     *
     * @return application-wide shared preferences.
     */
    public static SharedPreferences getAppSharedPreferences() {
        return Holder.sSharedPreferences;
    }

    /**
     * Occasionally tests cannot ensure the application context doesn't change between tests (junit)
     * and sometimes specific tests has its own special needs, initApplicationContext should be used
     * as much as possible, but this method can be used to override it.
     *
     * @param appContext The new application context.
     */
    public static void initApplicationContextForTests(Context appContext) {
        Context prevValue = sApplicationContext;
        initJavaSideApplicationContext(appContext);

        // initApplicationContext() lets <clinit> create sSharedPreferences, but that does not work
        // when setting it multiple times.
        SharedPreferences prevPrefs = Holder.sSharedPreferences;
        Holder.sSharedPreferences = fetchAppSharedPreferences();

        ResettersForTesting.register(
                () -> {
                    sApplicationContext = prevValue;
                    Holder.sSharedPreferences = prevPrefs;
                });
    }

    private static void initJavaSideApplicationContext(Context appContext) {
        assert appContext != null;
        // Guard against anyone trying to downcast.
        if (BuildConfig.ENABLE_ASSERTS && appContext instanceof Application) {
            appContext = new ContextWrapper(appContext);
        }
        sApplicationContext = appContext;
    }

    /**
     * @return Whether the process is isolated.
     */
    @SuppressWarnings("NewApi")
    public static boolean isIsolatedProcess() {
        // Was not made visible until Android P, but the method has always been there.
        return Process.isIsolated();
    }

    /**
     * @return if current process is SdkSandbox process.
     */
    public static boolean isSdkSandboxProcess() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            return Process.isSdkSandbox();
        } else {
            return false;
        }
    }

    /**
     * @return The name of the current process. E.g. "org.chromium.chrome:privileged_process0".
     */
    public static String getProcessName() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return Application.getProcessName();
        }
        try {
            Class<?> activityThreadClazz = Class.forName("android.app.ActivityThread");
            return (String) activityThreadClazz.getMethod("currentProcessName").invoke(null);
        } catch (Exception e) {
            // If fallback logic is ever needed, refer to:
            // https://chromium-review.googlesource.com/c/chromium/src/+/905563/1
            throw JavaUtils.throwUnchecked(e);
        }
    }

    /**
     * @return Whether the current process is 64-bit.
     */
    public static boolean isProcess64Bit() {
        return Process.is64Bit();
    }

    /**
     * Extract the {@link Activity} if the given {@link Context} either is or wraps one.
     *
     * @param context The context to check.
     * @return Extracted activity if it exists, otherwise null.
     */
    public static @Nullable Activity activityFromContext(@Nullable Context context) {
        // Only retrieves the base context if the supplied context is a ContextWrapper but not an
        // Activity, because Activity is a subclass of ContextWrapper.
        while (context instanceof ContextWrapper) {
            if (context instanceof Activity) return (Activity) context;

            context = ((ContextWrapper) context).getBaseContext();
        }

        return null;
    }

    /**
     * Register a broadcast receiver that may only accept protected broadcasts.
     *
     * You should (only) use this method when:
     * <p><ul>
     * <li>You need to receive protected broadcasts.
     * </ul><p>
     * This method does not presently verify that the provided IntentFilter covers only protected
     * broadcasts, so you should make sure that the broadcasts you register for are in fact
     * protected broadcasts. The Android platform's <a
     * href="https://android.googlesource.com/platform/frameworks/base/+/refs/heads/master/core/res/AndroidManifest.xml">
     * AndroidManifest.xml</a> contains a list of broadcasts which should be common to all devices.
     * You should be careful about using broadcasts which appear to be protected, but are not listed
     * in the platform's manifest, as they may not be protected on all devices. Different versions
     * or builds of Android may have different sets of protected broadcasts, so add appropriate
     * guards if needed.
     * <p>
     * You can unregister receivers using the normal {@link Context#unregisterReceiver} method.
     */
    public static Intent registerProtectedBroadcastReceiver(
            Context context, BroadcastReceiver receiver, IntentFilter filter) {
        return registerBroadcastReceiver(
                context, receiver, filter, /* permission= */ null, /* scheduler= */ null, 0);
    }

    public static Intent registerProtectedBroadcastReceiver(
            Context context, BroadcastReceiver receiver, IntentFilter filter, Handler scheduler) {
        return registerBroadcastReceiver(
                context, receiver, filter, /* permission= */ null, scheduler, 0);
    }

    /**
     * Register a broadcast receiver that may accept broadcasts from any UID.
     *
     * You should (only) use exported receivers when:
     * <p><ul>
     * <li>You need to receive unprotected broadcasts from other applications.
     * <li>Using unprotected sticky broadcasts - either from this application or another.
     * </ul><p>
     * Broadcasts received by exported receivers are untrustworthy and must be treated with caution.
     * <p>
     * You can unregister receivers using the normal {@link Context#unregisterReceiver} method.
     */
    public static Intent registerExportedBroadcastReceiver(
            Context context, BroadcastReceiver receiver, IntentFilter filter, String permission) {
        return registerBroadcastReceiver(
                context, receiver, filter, permission, /* scheduler= */ null, RECEIVER_EXPORTED);
    }

    /**
     * Register a broadcast receiver that may only accept broadcasts coming from the root, system,
     * or this app's own UIDs.
     *
     * You should generally prefer using this over the exported counterpart,
     * {@link #registerExportedBroadcastReceiver(Context, BroadcastReceiver, IntentFilter, String)},
     * unless you meet a specific requirement specified in that method's documentation.
     * <p>
     * Even though most protected broadcasts come from the system UID, and could thus be received by
     * a non-exported receiver, you should instead use registerProtectedBroadcastReceiver for all
     * protected broadcasts.
     * <p>
     * You should (only) use non-exported receivers when:
     * <p><ul>
     * <li>You want to send and receive (non-sticky) broadcasts solely within the same application.
     * <li>You want to receive the result of a PendingIntent that you have provided to some other
     * application or service.
     * </ul><p>
     * Note that older versions of Android do not enforce non-exported receivers, so you should
     * still not trust received Intents without some additional authentication mechanism. Note that
     * you generally cannot use Android permissions for this because embedded WebViews will only
     * inherit the permissions of the embedding application. Consider using
     * {@link org.chromium.base.IntentUtils#addTrustedIntentExtras} and
     * {@link org.chromium.base.IntentUtils#isTrustedIntentFromSelf} to verify the Intent's sender.
     * <p>
     * Usually, when working with non-exported receivers, you should also make sure that any related
     * Intents that you send are not broadcast to other apps. This can be done using
     * {@link Intent#setPackage} with {@link Context#getPackageName}, and must be done before
     * calling {@link org.chromium.base.IntentUtils#addTrustedIntentExtras}.
     * <p>
     * You can unregister receivers using the normal {@link Context#unregisterReceiver} method.
     */
    public static Intent registerNonExportedBroadcastReceiver(
            Context context, BroadcastReceiver receiver, IntentFilter filter) {
        return registerBroadcastReceiver(
                context,
                receiver,
                filter,
                /* permission= */ null,
                /* scheduler= */ null,
                RECEIVER_NOT_EXPORTED);
    }

    public static Intent registerNonExportedBroadcastReceiver(
            Context context, BroadcastReceiver receiver, IntentFilter filter, Handler scheduler) {
        return registerBroadcastReceiver(
                context,
                receiver,
                filter,
                /* permission= */ null,
                scheduler,
                RECEIVER_NOT_EXPORTED);
    }

    private static Intent registerBroadcastReceiver(
            Context context,
            BroadcastReceiver receiver,
            IntentFilter filter,
            String permission,
            Handler scheduler,
            int flags) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            return context.registerReceiver(receiver, filter, permission, scheduler, flags);
        } else {
            return context.registerReceiver(receiver, filter, permission, scheduler);
        }
    }
}
