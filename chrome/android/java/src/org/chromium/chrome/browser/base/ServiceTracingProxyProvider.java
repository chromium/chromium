// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import android.annotation.SuppressLint;
import android.app.ActivityManager;
import android.app.NotificationManager;
import android.content.Context;
import android.content.ContextWrapper;
import android.hardware.display.DisplayManager;
import android.media.AudioManager;
import android.media.MediaRouter;
import android.os.Build;
import android.os.IInterface;
import android.os.SystemClock;
import android.telephony.TelephonyManager;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.version_info.Channel;
import org.chromium.base.version_info.VersionConstants;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.ParameterizedType;
import java.lang.reflect.Proxy;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Proxies IInterfaces for System Services to add trace events for slow IPCs.
 *
 * <p>TODO(crbug.com/40850079): Support tracing system services cached in StaticServiceFetchers.
 * Right now we only support services cached per-context in CachedServiceFetchers.
 */
public class ServiceTracingProxyProvider {
    private static final String TAG = "TracingProxyProvider";

    // Don't trace events that are too short to avoid spamming traces.
    private static final long MINIMUM_IPC_TRACE_DURATION_MS = 2;
    private static final String TRACE_FAILED = "Failed to trace IPCs: ";
    private static final String PROXY_PREP_FAILED = "Failed to prepare service for proxying: ";

    private static final AtomicInteger sProxiesInstalled = new AtomicInteger();
    private static final AtomicInteger sProxiesAttempted = new AtomicInteger();
    private static final AtomicBoolean sProxyInstallCountHistogramRecorded = new AtomicBoolean();

    // Used to defeat Android's hidden API blocklist. I would tell you why it works, but the
    // truth is I don't know. Something to do with the calling class being loaded by the
    // boot classloader and double reflection.
    private static final Method sGetDeclaredMethod;
    private static final Method sGetMethod;
    private static final Method sGetDeclaredField;
    private static final Method sGetField;

    static {
        try {
            sGetDeclaredMethod =
                    Class.class.getDeclaredMethod("getDeclaredMethod", String.class, Class[].class);
            sGetMethod = Class.class.getDeclaredMethod("getMethod", String.class, Class[].class);
            sGetDeclaredField = Class.class.getDeclaredMethod("getDeclaredField", String.class);
            sGetField = Class.class.getDeclaredMethod("getField", String.class);
        } catch (Throwable e) {
            // These methods should always exist.
            throw new RuntimeException(e);
        }
    }

    private static final class IPCListener implements InvocationHandler {
        private final Object mSystemImpl;

        public IPCListener(Object systemImpl) {
            mSystemImpl = systemImpl;
        }

        @Override
        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            try {
                if (!ThreadUtils.runningOnUiThread()) return method.invoke(mSystemImpl, args);

                long start = SystemClock.elapsedRealtime();
                Object result = method.invoke(mSystemImpl, args);
                long durationMs = SystemClock.elapsedRealtime() - start;

                if (durationMs >= MINIMUM_IPC_TRACE_DURATION_MS) {
                    TraceEvent.instantAndroidIPC(
                            mSystemImpl.getClass().getName() + "#" + method.getName(), durationMs);
                }
                return result;
            } catch (InvocationTargetException e) {
                // Need to rethrow the cause or the proxy will generate
                // UndeclaredThrowableExceptions that callers won't be expecting.
                throw e.getCause();
            }
        }
    }

    // DO NOT MODIFY THIS ARRAY. This is a reference to the service cache in ContextImpl.
    private final Object[] mServiceCache;
    // Same length as |mServiceCache|, true if the corresponding service has been proxied.
    AtomicBoolean[] mServiceCacheProxied;

    private final Context mUnwrappedBaseContext;

    private static boolean isEnabled() {
        // A lot of service bindings were uncached pre-R, so easier to start tracing at R+.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return false;

        // Don't ship this tracing to Stable.
        if (VersionConstants.CHANNEL > Channel.BETA) return false;

        // static init failed.
        if (sGetDeclaredMethod == null) return false;
        return true;
    }

    /**
     * @param baseContext The base context for an Application/Activity.
     */
    public static @Nullable ServiceTracingProxyProvider create(Context baseContext) {
        if (!isEnabled()) return null;
        while (baseContext instanceof ContextWrapper) {
            baseContext = ((ContextWrapper) baseContext).getBaseContext();
        }
        return new ServiceTracingProxyProvider(baseContext);
    }

    private ServiceTracingProxyProvider(Context unwrappedBaseContext) {
        assert unwrappedBaseContext.getClass().getName().equals("android.app.ContextImpl");
        mUnwrappedBaseContext = unwrappedBaseContext;
        Object[] serviceCache;
        try {
            serviceCache =
                    (Object[])
                            getField(
                                    mUnwrappedBaseContext,
                                    mUnwrappedBaseContext.getClass(),
                                    "mServiceCache");
            mServiceCacheProxied = new AtomicBoolean[serviceCache.length];
            for (int i = 0; i < mServiceCacheProxied.length; ++i) {
                mServiceCacheProxied[i] = new AtomicBoolean(false);
            }
            // Force the window service to be accessed and added to the service cache immediately.
            // This will make sure it is proxied before ViewRootImpl can cache an unproxied
            // WindowSession.
            unwrappedBaseContext.getSystemService(Context.WINDOW_SERVICE);
        } catch (Throwable throwable) {
            Log.d(TAG, TRACE_FAILED, throwable);
            serviceCache = new Object[0];
        }
        mServiceCache = serviceCache;
    }

    public void traceSystemServices() {
        for (int i = 0; i < mServiceCache.length; ++i) {
            if (mServiceCache[i] != null && !mServiceCacheProxied[i].get()) {
                sProxiesAttempted.incrementAndGet();
                if (traceService(
                        mUnwrappedBaseContext, mServiceCache[i], mServiceCacheProxied[i])) {
                    sProxiesInstalled.incrementAndGet();
                }
            }
        }
        int attempts = sProxiesAttempted.get();
        if (attempts >= 40
                && sProxyInstallCountHistogramRecorded.compareAndSet(
                        /* expectedValue= */ false, true)) {
            RecordHistogram.recordSparseHistogram(
                    "Android.ServiceTracingProxyProvider.SuccessesOutOfInitialForty",
                    sProxiesInstalled.get());
        }
    }

    private static synchronized boolean traceService(
            Context context, Object service, AtomicBoolean serviceCacheProxied) {
        if (serviceCacheProxied.get()) return false;
        try {
            Log.d(TAG, "Attempting to proxy " + service.getClass().getName());
            service = prepareServiceForProxying(service);
        } catch (Throwable throwable) {
            Log.d(TAG, PROXY_PREP_FAILED, throwable);
        }
        boolean success = proxyService(context, service);
        serviceCacheProxied.set(true);
        if (!success) {
            Log.d(TAG, "Could not trace service: " + service.getClass().getName());
        }
        return success;
    }

    // Most services just store their interfaces as members in the service class, but some store
    // them in harder to find places, or don't initialize them at creation time.
    private static Object prepareServiceForProxying(Object service) throws Throwable {
        if (service.getClass().equals(DisplayManager.class)) {
            // Class defers to DisplayManagerGlobal.
            Class clazz = Class.forName("android.hardware.display.DisplayManagerGlobal");
            return callNoArgMethod(null, clazz, "getInstance");
        }
        if (service.getClass().getName().equals("android.view.WindowManagerImpl")) {
            // Class defers to WindowManagerGlobal.
            Class clazz = Class.forName("android.view.WindowManagerGlobal");
            Object managerGlobal = callNoArgMethod(null, clazz, "getInstance");
            // Static service and WindowSession are unpopulated until used. Access them now so that
            // the fields are populated when we inspect them for proxying.
            callNoArgMethod(null, managerGlobal.getClass(), "getWindowManagerService");
            callNoArgMethod(null, managerGlobal.getClass(), "getWindowSession");
            return managerGlobal;
        }
        if (service.getClass().equals(ActivityManager.class)) {
            // Service is stored in static singleton.
            Object singletonInstance =
                    getField(null, service.getClass(), "IActivityManagerSingleton");
            callNoArgMethod(singletonInstance, singletonInstance.getClass(), "get");
            return singletonInstance;
        }
        if (service.getClass().equals(NotificationManager.class)) {
            // Service member is unpopulated until used.
            callNoArgMethod(null, service.getClass(), "getService");
            return service;
        }
        if (service.getClass().equals(TelephonyManager.class)) {
            // Service member is unpopulated until used.
            try {
                callNoArgMethod(null, service.getClass(), "getSubscriberInfoService");
            } catch (Throwable e) {
                Log.d(TAG, PROXY_PREP_FAILED, e);
            }
            try {
                callNoArgMethod(null, service.getClass(), "getSubscriptionService");
            } catch (Throwable e) {
                Log.d(TAG, PROXY_PREP_FAILED, e);
            }
            try {
                callNoArgMethod(null, service.getClass(), "getSmsService");
            } catch (Throwable e) {
                Log.d(TAG, PROXY_PREP_FAILED, e);
            }
            try {
                callNoArgMethod(service, service.getClass(), "getITelephony");
            } catch (Throwable e) {
                Log.d(TAG, PROXY_PREP_FAILED, e);
            }
            return service;
        }
        if (service.getClass().equals(MediaRouter.class)) {
            // Service is stored in static singleton.
            return getField(null, service.getClass(), "sStatic");
        }
        if (service.getClass().equals(AudioManager.class)) {
            // Static service is unpopulated until used.
            callNoArgMethod(null, service.getClass(), "getService");
            return service;
        }
        return service;
    }

    private static Object callNoArgMethod(Object instance, Class<?> clazz, String methodName)
            throws Exception {
        Method method;
        try {
            method = (Method) sGetDeclaredMethod.invoke(clazz, methodName, null);
        } catch (Throwable e) {
            method = (Method) sGetMethod.invoke(clazz, methodName, null);
        }
        method.setAccessible(true);
        return method.invoke(instance);
    }

    private static Object getField(Object instance, Class<?> clazz, String fieldName)
            throws Exception {
        Field field;
        try {
            field = (Field) sGetDeclaredField.invoke(clazz, fieldName);
        } catch (Throwable e) {
            field = (Field) sGetField.invoke(clazz, fieldName);
        }
        field.setAccessible(true);
        return field.get(instance);
    }

    @SuppressLint("NewApi") // Class requires API level 30.
    private static boolean proxyService(Context context, Object service) {
        boolean ret = false;
        try {
            // Search through the class's fields to find Interfaces for Binders.
            Field[] fields = service.getClass().getDeclaredFields();
            // For generic classes like Singleton<? implements IInterface> we need to get the
            // fields from the superclass. Note that if the types are defined on the class itself
            // and not the superclass, it's impossible to get them.
            boolean isGenericClass =
                    service.getClass().getGenericSuperclass() instanceof ParameterizedType;
            // For simplicity, only check the first generic type.
            String genericTypeName = "";
            if (isGenericClass) {
                fields = service.getClass().getSuperclass().getDeclaredFields();
                genericTypeName =
                        service.getClass().getSuperclass().getTypeParameters()[0].getName();
            }
            for (Field field : fields) {
                field.setAccessible(true);
                Class<?> type;
                if (isGenericClass) {
                    if (!field.getGenericType().getTypeName().equals(genericTypeName)) continue;
                    type =
                            (Class<?>)
                                    ((ParameterizedType) service.getClass().getGenericSuperclass())
                                            .getActualTypeArguments()[0];
                } else {
                    type = field.getType();
                }
                if (IInterface.class.isAssignableFrom(type) && type.isInterface()) {
                    Object impl = field.get(service);
                    if (impl == null) {
                        Log.d(TAG, TRACE_FAILED + type + " is null");
                        continue;
                    }
                    // Avoid double-proxying for shared/static bindings.
                    if (Proxy.isProxyClass(impl.getClass())) continue;
                    Object listener =
                            Proxy.newProxyInstance(
                                    context.getClassLoader(),
                                    new Class<?>[] {type},
                                    new IPCListener(impl));
                    field.set(service, listener);
                    Log.d(TAG, "Tracing Proxy installed on: " + type);
                    ret = true;
                }
            }
        } catch (Throwable throwable) {
            Log.d(TAG, TRACE_FAILED, throwable);
        }
        return ret;
    }
}
