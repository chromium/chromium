// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Binder;
import android.os.IBinder;
import android.os.RemoteException;

import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import androidx.annotation.VisibleForTesting;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.function.BiConsumer;

/**
 * Installs a listener for all UI thread Binder calls, and adds a TraceEvent for each one.
 *
 * <p>This relies on undocumented Android APIs, meaning that it may break in the future. It also
 * means that installing the hook is not guaranteed to be successful. On the bright side, this
 * covers all Binder calls made through BinderProxy, which covers Chromium code, as well as
 * third-party and framework code.
 */
public class BinderCallsListener {
    private static final String TAG = "BinderCallsListener";
    private static final String PROXY_TRANSACT_LISTENER_CLASS_NAME =
            "android.os.Binder$ProxyTransactListener";

    private static BinderCallsListener sInstance;

    private Object mImplementation;
    private InterfaceInvocationHandler mInvocationHandler;
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

    private boolean installListener(Object listener) {
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
        private String mCurrentInterfaceDescriptor;
        private BiConsumer<String, String> mObserver;

        @Override
        public Object invoke(Object proxy, Method method, Object[] args) {
            if (!ThreadUtils.runningOnUiThread()) return null;
            switch (method.getName()) {
                case "onTransactStarted":
                    IBinder binder = (IBinder) args[0];
                    try {
                        mCurrentInterfaceDescriptor = binder.getInterfaceDescriptor();
                    } catch (RemoteException e) {
                        mCurrentInterfaceDescriptor = null;
                    }

                    TraceEvent.begin("BinderCallsListener.invoke", mCurrentInterfaceDescriptor);
                    if (mObserver != null) {
                        mObserver.accept("onTransactStarted", mCurrentInterfaceDescriptor);
                    }
                    return null;
                case "onTransactEnded":
                    TraceEvent.end("BinderCallsListener.invoke", mCurrentInterfaceDescriptor);

                    if (mObserver != null) {
                        mObserver.accept("onTransactEnded", mCurrentInterfaceDescriptor);
                    }
                    return null;
            }
            return null;
        }
    }
}
