// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.Activity;
import android.content.ComponentCallbacks2;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.memory.MemoryPressureCallback;

/**
 * This class is Java equivalent of base::MemoryPressureListener: it distributes pressure
 * signals to callbacks.
 *
 * The class also serves as an entry point to the native side - once native code is ready,
 * it adds native callback.
 *
 * notifyMemoryPressure() is called exclusively by MemoryPressureMonitor, which
 * monitors and throttles pressure signals.
 *
 * NOTE: this class should only be used on UiThread as defined by ThreadUtils (which is
 *       Android main thread for Chrome, but can be some other thread for WebView).
 */
@MainDex
public class MemoryPressureListener {
    /**
     * Sending an intent with this action to Chrome will cause it to issue a call to onLowMemory
     * thus simulating a low memory situations.
     */
    private static final String ACTION_LOW_MEMORY = "org.chromium.base.ACTION_LOW_MEMORY";

    /**
     * Sending an intent with this action to Chrome will cause it to issue a call to onTrimMemory
     * thus simulating a low memory situations.
     */
    private static final String ACTION_TRIM_MEMORY = "org.chromium.base.ACTION_TRIM_MEMORY";

    /**
     * Sending an intent with this action to Chrome will cause it to issue a call to onTrimMemory
     * with notification level TRIM_MEMORY_RUNNING_CRITICAL thus simulating a low memory situation
     */
    private static final String ACTION_TRIM_MEMORY_RUNNING_CRITICAL =
            "org.chromium.base.ACTION_TRIM_MEMORY_RUNNING_CRITICAL";

    /**
     * Sending an intent with this action to Chrome will cause it to issue a call to onTrimMemory
     * with notification level TRIM_MEMORY_MODERATE thus simulating a low memory situation
     */
    private static final String ACTION_TRIM_MEMORY_MODERATE =
            "org.chromium.base.ACTION_TRIM_MEMORY_MODERATE";

    private static final ObserverList<MemoryPressureCallback> sCallbacks = new ObserverList<>();

    /**
     * Called by the native side to add native callback.
     */
    @CalledByNative
    private static void addNativeCallback() {
        addCallback((pressure) -> MemoryPressureListenerJni.get().onMemoryPressure(pressure));
    }

    /**
     * Adds a memory pressure callback.
     * Callback is only added once, regardless of the number of addCallback() calls.
     * This method should be called only on ThreadUtils.UiThread.
     */
    public static void addCallback(MemoryPressureCallback callback) {
        sCallbacks.addObserver(callback);
    }

    /**
     * Removes previously added memory pressure callback.
     * This method should be called only on ThreadUtils.UiThread.
     */
    public static void removeCallback(MemoryPressureCallback callback) {
        sCallbacks.removeObserver(callback);
    }

    /**
     * Distributes |pressure| to all callbacks.
     * This method should be called only on ThreadUtils.UiThread.
     */
    public static void notifyMemoryPressure(@MemoryPressureLevel int pressure) {
        for (MemoryPressureCallback callback : sCallbacks) {
            callback.onPressure(pressure);
        }
    }

    /**
     * Used by applications to simulate a memory pressure signal. By throwing certain intent
     * actions.
     */
    public static boolean handleDebugIntent(Activity activity, String action) {
        if (ACTION_LOW_MEMORY.equals(action)) {
            simulateLowMemoryPressureSignal(activity);
        } else if (ACTION_TRIM_MEMORY.equals(action)) {
            simulateTrimMemoryPressureSignal(activity, ComponentCallbacks2.TRIM_MEMORY_COMPLETE);
        } else if (ACTION_TRIM_MEMORY_RUNNING_CRITICAL.equals(action)) {
            simulateTrimMemoryPressureSignal(activity,
                    ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL);
        } else if (ACTION_TRIM_MEMORY_MODERATE.equals(action)) {
            simulateTrimMemoryPressureSignal(activity, ComponentCallbacks2.TRIM_MEMORY_MODERATE);
        } else {
            return false;
        }

        return true;
    }

    private static void simulateLowMemoryPressureSignal(Activity activity) {
        // The Application and the Activity each have a list of callbacks they notify when this
        // method is called.  Notifying these will simulate the event at the App/Activity level
        // as well as trigger the listener bound from native in this process.
        activity.getApplication().onLowMemory();
        activity.onLowMemory();
    }

    private static void simulateTrimMemoryPressureSignal(Activity activity, int level) {
        // The Application and the Activity each have a list of callbacks they notify when this
        // method is called.  Notifying these will simulate the event at the App/Activity level
        // as well as trigger the listener bound from native in this process.
        activity.getApplication().onTrimMemory(level);
        activity.onTrimMemory(level);
    }

    @NativeMethods
    interface Natives {
        void onMemoryPressure(@MemoryPressureLevel int pressure);
    }
}
