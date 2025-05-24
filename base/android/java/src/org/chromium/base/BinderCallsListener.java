// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Binder;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.SystemClock;

import androidx.annotation.UiThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.Collections;
import java.util.HashSet;
import java.util.function.BiConsumer;

/**
 * Installs a listener for all UI thread Binder calls, and adds a TraceEvent for each one.
 *
 * <p>This relies on undocumented Android APIs, meaning that it may break in the future. It also
 * means that installing the hook is not guaranteed to be successful. On the bright side, this
 * covers all Binder calls made through BinderProxy, which covers Chromium code, as well as
 * third-party and framework code.
 */
@NullMarked
public class BinderCallsListener {
    private static final String TAG = "BinderCallsListener";
    private static final String PROXY_TRANSACT_LISTENER_CLASS_NAME =
            "android.os.Binder$ProxyTransactListener";
    private static final String NON_ANDROID_INTERFACE = "NON_ANDROID_INTERFACE";
    private static final String EMPTY_INTERFACE = "EMPTY_INTERFACE";
    private static final String NULL_INTERFACE = "NULL_INTERFACE";
    private static final String UNKNOWN_INTERFACE = "UNKNOWN_INTERFACE";

    private static @Nullable BinderCallsListener sInstance;

    private static final long LONG_BINDER_CALL_LIMIT_MILLIS = 2;
    private static final HashSet<String> sSlowBinderCallAllowList = new HashSet<>();

    // The comments mostly correspond to the slow use cases.
    static {
        Collections.addAll(
                sSlowBinderCallAllowList,
                // Callbacks for lifecycle events.
                "android.app.IActivityTaskManager",
                // Used for getActivityInfo, hasSystemFeature, and getSystemAvailableFeatures calls.
                "android.content.pm.IPackageManager",
                // Called by Choreographer.
                "android.view.IWindowSession",
                // Check whether UI mode is TV.
                "android.app.IUiModeManager",
                // Used to add Incognito launcher shortcut.
                "android.content.pm.IShortcutService",
                // Interactions with activities, services and content providers.
                "android.app.IActivityManager",
                // Used for getService calls.
                "android.os.IServiceManager",
                // Checks if power saving mode is enabled.
                "android.os.IPowerManager",
                // Used by Android code.
                "android.content.IContentProvider",
                "android.view.accessibility.IAccessibilityInteractionConnectionCallback",
                "android.view.accessibility.IAccessibilityManager",
                "android.view.contentcapture.IContentCaptureManager",
                "android.os.IUserManager",
                "android.hardware.devicestate.IDeviceStateManager",
                "com.android.internal.telephony.ISub",
                "com.android.internal.app.IAppOpsService",
                "com.android.internal.app.IBatteryStats",
                "android.view.IGraphicsStats",
                "android.app.job.IJobCallback",
                "android.app.trust.ITrustManager",
                "android.media.IAudioService",
                "com.android.internal.inputmethod.IImeTracker",
                "com.android.internal.inputmethod.IInputMethodSession",
                "com.android.internal.app.IVoiceInteractionManagerService",
                "com.android.internal.textservice.ITextServicesManager",
                // See https://crbug.com/407479092.
                "com.android.internal.telephony.ITelephony",
                "com.android.internal.infra.IAndroidFuture",
                "com.android.internal.textservice.ISpellCheckerSession",
                "com.android.internal.telecom.ITelecomService",
                // Gets activity task ID during startup; cached.
                "android.app.IActivityClientController",
                // Used to check if stylus is enabled.
                "com.android.internal.view.IInputMethodManager",
                // Updates cursor anchor info - https://crbug.com/407792620.
                "com.android.internal.view.IInputMethodSession",
                // Registers content observers.
                "android.content.IContentService",
                // BackgroundTaskScheduler.
                "android.app.job.IJobScheduler",
                // ConnectivitiyManager#getNetworkInfo.
                "android.net.IConnectivityManager",
                // Used by NetworkActiveNotifier to change the underlying network state -
                // https://crbug.com/407478223.
                "android.net.ITetheringConnector",
                // Used to get Window Insets.
                "android.view.IWindowManager",
                // Determines if specific permissions are revoked by policy.
                "android.permission.IPermissionManager",
                // Used to get the system locale to set the UI language.
                "android.app.ILocaleManager",
                // Gets all search widget IDs.
                "com.android.internal.appwidget.IAppWidgetService",
                // Registers HDR:SDR change listener.
                "android.hardware.display.IDisplayManager",
                // Register Clipboard listener.
                "android.content.IClipboard",
                // Register input device change listener.
                "android.hardware.input.IInputManager",
                // Creates notification channels for devices on Android O and above.
                "android.app.INotificationManager",
                // AppTask#getTaskInfo
                "android.app.IAppTask",
                // Used to determine if device can authenticate with a given level of strength.
                "android.hardware.biometrics.IAuthService",
                // Context#getExternalFilesDirs for download directories.
                "android.os.storage.IStorageManager",
                // Watches changes to Android prefs backed up using Android KV backup.
                "android.app.backup.IBackupManager",
                // Used in test screenshots.
                "android.app.IUiAutomationConnection",
                // PowerMonitor#getCurrentThermalStatus.
                "android.os.IThermalService",
                // StrictMode#setVmPolicy. Only enabled for local builds and 1% of Dev users.
                "android.os.INetworkManagementService",
                // Records background restrictions imposed on Chrome by Android.
                "android.app.usage.IUsageStatsManager",
                // Used for EditText UI elements.
                "android.view.autofill.IAutoFillManager",
                // Used in MediaNotificationController - https://crbug.com/407575141.
                "android.media.session.ISession",
                // Used in LocationUtils to check if location is enabled -
                // https://crbug.com/407576192.
                "android.location.ILocationManager",
                // Called when PhysicalDisplayAndroid creates the window context -
                // https://crbug.com/407495299.
                "android.companion.virtual.IVirtualDeviceManager",
                // Stopping TextToSpeech - https://crbug.com/407493249.
                "android.speech.tts.ITextToSpeechService",
                // Creation of android.speech.tts.TextToSpeech - https://crbug.com/407618827.
                "android.speech.tts.ITextToSpeechManager",
                // Wraps CCT callbacks with a CustomTabsConnection#safeExtraCallback -
                // https://crbug.com/407696847.
                "android.support.customtabs.ICustomTabsCallback",
                // CCT scroll events - https://crbug.com/407591642.
                "android.support.customtabs.IEngagementSignalsCallback",
                // Called onWindowFocusChanged - https://crbug.com/407570292.
                "android.app.unipnp.IUnionManager",
                // Checks if the Browser role is available to promote dialogs -
                // https://crbug.com/407477867.
                "android.app.role.IRoleManager",
                // Quick Delete's haptic feedback - https://crbug.com/407955365.
                "android.os.IVibratorService",
                // Creates Smart Selection session - https://crbug.com/407821966.
                "android.service.textclassifier.ITextClassifierService",
                // Checks if Advanced Protection is enabled - https://crbug.com/407749727.
                "android.security.advancedprotection.IAdvancedProtectionService",
                // Web APK Notification permissions check - https://crbug.com/407749507.
                "org.chromium.webapk.lib.runtime_library.IWebApkApi",
                // Creating media sessions & router service - https://crbug.com/417686302.
                "android.media.session.ISessionManager",
                "android.media.IMediaRouterService");
    }

    private @Nullable Object mImplementation;
    private @Nullable InterfaceInvocationHandler mInvocationHandler;
    private boolean mInstalled;

    @UiThread
    public static @Nullable BinderCallsListener getInstance() {
        ThreadUtils.assertOnUiThread();

        if (sInstance == null) sInstance = new BinderCallsListener();
        return sInstance;
    }

    private BinderCallsListener() {
        try {
            Class interfaceClass = Class.forName(PROXY_TRANSACT_LISTENER_CLASS_NAME);
            mInvocationHandler = new InterfaceInvocationHandler();
            Object implementation =
                    Proxy.newProxyInstance(
                            interfaceClass.getClassLoader(),
                            new Class[] {interfaceClass},
                            mInvocationHandler);
            mImplementation = implementation;
        } catch (Exception e) {
            // Undocumented API, do not fail if it changes.
            Log.w(TAG, "Failed to create the listener proxy. Has the framework changed?");
        }
    }

    public static void setInstanceForTesting(BinderCallsListener testInstance) {
        if (sInstance != null && testInstance != null) {
            throw new IllegalStateException("A real instance already exists.");
        }

        sInstance = testInstance;
    }

    /**
     * Tries to install the listener. Must be called on the UI thread. May not succeed, and may be
     * called several times.
     */
    @UiThread
    public boolean installListener() {
        return installListener(mImplementation);
    }

    /**
     * Get the total time spent in Binder calls since the listener was installed, if it was
     * installed.
     *
     * <p>NOTE: The current implementation of BinderCallsListener is *very* exhaustive. This method
     * will include time spent in Binder calls that originated from outside of Chrome too.
     *
     * @return time spent in Binder calls in milliseconds since the listener was installed or null.
     */
    @Nullable
    public Long getTimeSpentInBinderCalls() {
        if (!mInstalled || mInvocationHandler == null) return null;
        return mInvocationHandler.getTimeSpentInBinderCalls();
    }

    private boolean installListener(@Nullable Object listener) {
        if (mInstalled) return false;

        try {
            // Used to defeat Android's hidden API blocklist. Taken from
            // chrome/browser/base/ServiceTracingProxyProvider.java, see there for details on why
            // this uses reflection twice.
            Method getMethod =
                    Class.class.getDeclaredMethod("getMethod", String.class, Class[].class);
            Method m =
                    (Method)
                            getMethod.invoke(
                                    Binder.class,
                                    "setProxyTransactListener",
                                    new Class<?>[] {
                                        Class.forName(PROXY_TRANSACT_LISTENER_CLASS_NAME)
                                    });
            assert m != null;
            m.invoke(null, listener);
        } catch (ClassNotFoundException
                | InvocationTargetException
                | IllegalAccessException
                | NoSuchMethodException e) {
            // Not critical to install the listener, swallow the exception.
            Log.w(TAG, "Failed to install the Binder listener");
            return false;
        }

        Log.d(TAG, "Successfully installed the Binder listener");
        mInstalled = true;
        return true;
    }

    @VisibleForTesting
    void setBinderCallListenerObserverForTesting(BiConsumer<String, String> observer) {
        if (mInvocationHandler != null) mInvocationHandler.mObserver = observer;
    }

    private static class InterfaceInvocationHandler implements InvocationHandler {
        private String mCurrentInterfaceDescriptor = EMPTY_INTERFACE;
        private @Nullable BiConsumer<String, String> mObserver;
        private int mCurrentTransactionId;
        private long mTotalTimeSpentInBinderCallsMillis;
        private long mCurrentTransactionStartTimeMillis;

        private static boolean isAndroidBinderInterface(String interfaceDescriptor) {
            return (interfaceDescriptor.startsWith("com.android.")
                            && !interfaceDescriptor.startsWith("com.android.vending"))
                    || interfaceDescriptor.startsWith("android.");
        }

        private String getInterfaceDescriptor(IBinder binder) {
            try {
                String interfaceDescriptor = binder.getInterfaceDescriptor();
                return interfaceDescriptor == null
                        ? NULL_INTERFACE
                        : (interfaceDescriptor.isEmpty() ? EMPTY_INTERFACE : interfaceDescriptor);
            } catch (RemoteException e) {
                Log.w(TAG, "Unable to read interface descriptor.");
            }
            return UNKNOWN_INTERFACE;
        }

        public long getTimeSpentInBinderCalls() {
            return mTotalTimeSpentInBinderCallsMillis;
        }

        @Override
        public @Nullable Object invoke(Object proxy, Method method, Object[] args) {
            if (!ThreadUtils.runningOnUiThread()) return null;
            switch (method.getName()) {
                case "onTransactStarted":
                    IBinder binder = (IBinder) args[0];
                    mCurrentTransactionId++;
                    mCurrentTransactionStartTimeMillis = SystemClock.uptimeMillis();

                    mCurrentInterfaceDescriptor = getInterfaceDescriptor(binder);
                    // If we failed to read the interface descriptor, ignore it.
                    if (mCurrentInterfaceDescriptor.equals(UNKNOWN_INTERFACE)) {
                        return null;
                    }
                    boolean shouldTrackBinderIpc =
                            !sSlowBinderCallAllowList.contains(mCurrentInterfaceDescriptor);
                    if (!isAndroidBinderInterface(mCurrentInterfaceDescriptor)) {
                        mCurrentInterfaceDescriptor = NON_ANDROID_INTERFACE;
                        shouldTrackBinderIpc = false;
                    }

                    TraceEvent.begin("BinderCallsListener.invoke", mCurrentInterfaceDescriptor);
                    if (mObserver != null) {
                        mObserver.accept("onTransactStarted", mCurrentInterfaceDescriptor);
                    }

                    return shouldTrackBinderIpc ? mCurrentTransactionId : null;
                case "onTransactEnded":
                    TraceEvent.end("BinderCallsListener.invoke", mCurrentInterfaceDescriptor);

                    long transactionDurationMillis =
                            SystemClock.uptimeMillis() - mCurrentTransactionStartTimeMillis;
                    mTotalTimeSpentInBinderCallsMillis += transactionDurationMillis;
                    if (mObserver != null) {
                        assert mCurrentInterfaceDescriptor != null;
                        mObserver.accept("onTransactEnded", mCurrentInterfaceDescriptor);
                    }

                    Integer session = (Integer) args[0];
                    if (session == null || session != mCurrentTransactionId) {
                        return null;
                    }

                    boolean shouldReportSlowCall =
                            transactionDurationMillis >= LONG_BINDER_CALL_LIMIT_MILLIS;
                    if (shouldReportSlowCall) {
                        // If there was a new Binder call introduced, consider moving it to a
                        // background thread if possible. If not, add it to the allow list.
                        String message =
                                "This is not a crash. BinderCallsListener detected a slow call on"
                                        + " the UI thread by: "
                                        + mCurrentInterfaceDescriptor
                                        + " with duration="
                                        + transactionDurationMillis
                                        + "ms (max allowed: "
                                        + LONG_BINDER_CALL_LIMIT_MILLIS
                                        + "ms)";
                        Log.w(TAG, message);
                    }
                    return null;
            }
            return null;
        }
    }
}
