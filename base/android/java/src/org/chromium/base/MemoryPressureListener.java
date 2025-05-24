// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.ComponentCallbacks2;
import android.os.Handler;
import android.os.Looper;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.memory.MemoryPressureCallback;
import org.chromium.base.memory.SelfFreezeCallback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * This class is Java equivalent of base::MemoryPressureListener: it distributes signals to
 * callbacks for memory pressure, pre-freeze, and self-freeze.
 *
 * <p>The class also serves as an entry point to the native side - once native code is ready, it
 * adds native callback.
 *
 * <p>notifyMemoryPressure() is called exclusively by MemoryPressureMonitor, which monitors and
 * throttles pressure signals.
 *
 * <p>NOTE: this class should only be used on UiThread as defined by ThreadUtils (which is Android
 * main thread for Chrome, but can be some other thread for WebView), except for the self freeze
 * calls, which are done on the launcher thread.
 */
@NullMarked
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

    private static @Nullable ObserverList<MemoryPressureCallback> sCallbacks;
    // This is used only on the Launcher thread.
    private static @Nullable ObserverList<SelfFreezeCallback> sSelfFreezeCallbacks;
    private static volatile @Nullable Handler sSelfFreezeHandler;

    /** Called by the native side to add native callback. */
    @CalledByNative
    private static void addNativeCallback() {
        ThreadUtils.assertOnUiThread();
        addCallback((pressure) -> MemoryPressureListenerJni.get().onMemoryPressure(pressure));
    }

    /**
     * Adds a memory pressure callback.
     * Callback is only added once, regardless of the number of addCallback() calls.
     * This method should be called only on ThreadUtils.UiThread.
     */
    public static void addCallback(MemoryPressureCallback callback) {
        ThreadUtils.assertOnUiThread();
        if (sCallbacks == null) sCallbacks = new ObserverList<>();
        sCallbacks.addObserver(callback);
    }

    /** Adds a self freeze callback. This method is called only on the Launcher thread. */
    public static void addSelfFreezeCallback(SelfFreezeCallback callback) {
        if (sSelfFreezeCallbacks == null) sSelfFreezeCallbacks = new ObserverList<>();
        sSelfFreezeCallbacks.addObserver(callback);

        if (sSelfFreezeHandler == null) {
            sSelfFreezeHandler = new Handler();
            // We just added a single element, above.
            assert sSelfFreezeCallbacks.size() == 1;
        }
        assert sSelfFreezeHandler.getLooper() == Looper.myLooper();
    }

    /**
     * Removes previously added memory pressure callback. This method should be called only on
     * ThreadUtils.UiThread.
     */
    public static void removeCallback(MemoryPressureCallback callback) {
        ThreadUtils.assertOnUiThread();
        if (sCallbacks == null) return;
        sCallbacks.removeObserver(callback);
    }

    public static void removeSelfFreezeCallback(SelfFreezeCallback callback) {
        assert assumeNonNull(sSelfFreezeHandler).getLooper() == Looper.myLooper();
        assumeNonNull(sSelfFreezeCallbacks).removeObserver(callback);
    }

    /**
     * Distributes |pressure| to all callbacks. This method should be called only on
     * ThreadUtils.UiThread.
     *
     * <p>This includes sending the notification to the native side, provided that
     * addNativeCallback() has been called. It does not trigger all the clients listening directly
     * to ComponentCallbacks2 notifications.
     */
    public static void notifyMemoryPressure(@MemoryPressureLevel int pressure) {
        ThreadUtils.assertOnUiThread();
        if (sCallbacks == null) return;
        for (MemoryPressureCallback callback : sCallbacks) {
            callback.onPressure(pressure);
        }
    }

    public static void notifySelfFreeze() {
        ThreadUtils.assertOnUiThread();
        if (sSelfFreezeHandler != null) {
            sSelfFreezeHandler.post(
                    () -> {
                        if (sSelfFreezeCallbacks == null) return;
                        for (SelfFreezeCallback callback : sSelfFreezeCallbacks) {
                            callback.onSelfFreeze();
                        }
                    });
        }
    }

    public static void onPreFreeze() {
        // We only need the library to be loaded, not the whole browser
        // to be initialized because the native side would have no tasks
        // to run in this case (they are registered at various points by
        // callers elsewhere).
        if (!LibraryLoader.getInstance().isInitialized()) return;
        MemoryPressureListenerJni.get().onPreFreeze();
    }

    public static boolean isTrimMemoryBackgroundCritical() {
        if (!LibraryLoader.getInstance().isInitialized()) return false;
        return MemoryPressureListenerJni.get().isTrimMemoryBackgroundCritical();
    }

    /**
     * Used by applications to simulate a memory pressure signal. By throwing certain intent
     * actions.
     */
    public static boolean handleDebugIntent(Activity activity, String action) {
        ThreadUtils.assertOnUiThread();
        if (ACTION_LOW_MEMORY.equals(action)) {
            simulateLowMemoryPressureSignal(activity);
        } else if (ACTION_TRIM_MEMORY.equals(action)) {
            simulateTrimMemoryPressureSignal(activity, ComponentCallbacks2.TRIM_MEMORY_COMPLETE);
        } else if (ACTION_TRIM_MEMORY_RUNNING_CRITICAL.equals(action)) {
            simulateTrimMemoryPressureSignal(
                    activity, ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL);
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

        void onPreFreeze();

        boolean isTrimMemoryBackgroundCritical();
    }
}
