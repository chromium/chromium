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
                "android.view.accessibility.IAccessibilityManager",
                "android.os.IUserManager",
                "android.hardware.devicestate.IDeviceStateManager",
                "com.android.internal.telephony.ISub",
                "com.android.internal.app.IAppOpsService",
                "android.view.IGraphicsStats",
                "android.app.job.IJobCallback",
                "android.app.trust.ITrustManager",
                "android.media.IAudioService",
                // Gets activity task ID during startup; cached.
                "android.app.IActivityClientController",
                // Used to check if stylus is enabled.
                "com.android.internal.view.IInputMethodManager",
                // Registers content observers.
                "android.content.IContentService",
                // BackgroundTaskScheduler.
                "android.app.job.IJobScheduler",
                // ConnectivitiyManager#getNetworkInfo.
                "android.net.IConnectivityManager",
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
                "android.hardware.biometrics.IAuthService");
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
            // Undocumented API, do not fail if it changes. Pretend that it has been installed
            // to not attempt it later.
            Log.w(TAG, "Failed to create the listener proxy. Has the framework changed?");
            mInstalled = true;
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
        private @Nullable String mCurrentInterfaceDescriptor;
        private @Nullable BiConsumer<String, String> mObserver;
        private int mCurrentTransactionId;
        private long mCurrentTransactionStartTimeMillis;

        @Override
        public @Nullable Object invoke(Object proxy, Method method, Object[] args) {
            if (!ThreadUtils.runningOnUiThread()) return null;
            switch (method.getName()) {
                case "onTransactStarted":
                    IBinder binder = (IBinder) args[0];
                    mCurrentTransactionId++;
                    mCurrentTransactionStartTimeMillis = SystemClock.uptimeMillis();
                    try {
                        mCurrentInterfaceDescriptor = binder.getInterfaceDescriptor();
                    } catch (RemoteException e) {
                        mCurrentInterfaceDescriptor = null;
                        return null;
                    }

                    TraceEvent.begin("BinderCallsListener.invoke", mCurrentInterfaceDescriptor);
                    if (mObserver != null) {
                        mObserver.accept("onTransactStarted", mCurrentInterfaceDescriptor);
                    }
                    if (!sSlowBinderCallAllowList.contains(mCurrentInterfaceDescriptor)) {
                        return mCurrentTransactionId;
                    }
                    return null;
                case "onTransactEnded":
                    TraceEvent.end("BinderCallsListener.invoke", mCurrentInterfaceDescriptor);
                    if (mObserver != null) {
                        mObserver.accept("onTransactEnded", mCurrentInterfaceDescriptor);
                    }

                    Integer session = (Integer) args[0];
                    if (session == null || session != mCurrentTransactionId) {
                        return null;
                    }

                    long transactionDurationMillis =
                            SystemClock.uptimeMillis() - mCurrentTransactionStartTimeMillis;
                    if (transactionDurationMillis >= LONG_BINDER_CALL_LIMIT_MILLIS) {
                        Log.e(
                                TAG,
                                "Slow call on UI thread by: %s duration=%dms (max allowed: %dms)",
                                mCurrentInterfaceDescriptor,
                                transactionDurationMillis,
                                LONG_BINDER_CALL_LIMIT_MILLIS);
                    }
                    return null;
            }
            return null;
        }
    }
}
