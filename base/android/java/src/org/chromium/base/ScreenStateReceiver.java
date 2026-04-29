// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A {@link BroadcastReceiver} that listens for screen state changes (off/on).
 *
 * <p>This class is a singleton that is never destroyed as it is an application level broadcast
 * receiver. Care should be taken to avoid memory leaks by removing listeners when they are no
 * longer needed.
 */
@NullMarked
public class ScreenStateReceiver extends BroadcastReceiver {
    /** A listener for screen state broadcasts. */
    public interface ScreenStateObserver {
        default void onScreenOff(Context context, Intent intent) {}

        default void onScreenOn(Context context, Intent intent) {}
    }

    // Initialize lazily to facilitate testing.
    private static @Nullable ScreenStateReceiver sInstance;

    private final ObserverList<ScreenStateObserver> mObservers = new ObserverList<>();
    private final TaskRunner mTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.USER_VISIBLE);

    private ScreenStateReceiver() {}

    /**
     * Adds an observer for screen state broadcasts.
     *
     * @param observer The observer to add.
     */
    public static void addObserver(ScreenStateObserver observer) {
        ThreadUtils.assertOnUiThread();
        getInstance().mObservers.addObserver(observer);
    }

    /**
     * Removes an observer for screen state broadcasts.
     *
     * @param observer The observer to remove.
     */
    public static void removeObserver(ScreenStateObserver observer) {
        ThreadUtils.assertOnUiThread();
        getInstance().mObservers.removeObserver(observer);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        ThreadUtils.assertOnUiThread();
        if (Intent.ACTION_SCREEN_OFF.equals(intent.getAction())) {
            for (ScreenStateObserver observer : mObservers) {
                observer.onScreenOff(context, intent);
            }
        } else if (Intent.ACTION_SCREEN_ON.equals(intent.getAction())) {
            for (ScreenStateObserver observer : mObservers) {
                observer.onScreenOn(context, intent);
            }
        }
    }

    /** Returns the singleton instance of the {@link ScreenStateReceiver}. */
    @VisibleForTesting
    public static ScreenStateReceiver getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new ScreenStateReceiver();
            sInstance.register();
        }
        return sInstance;
    }

    private void register() {
        // Registering a broadcast receiver can be slow. To prevent blocking do this asynchronously.
        mTaskRunner.postDelayedTask(
                () -> {
                    IntentFilter filter = new IntentFilter();
                    filter.addAction(Intent.ACTION_SCREEN_OFF);
                    filter.addAction(Intent.ACTION_SCREEN_ON);
                    ContextUtils.registerProtectedBroadcastReceiver(
                            ContextUtils.getApplicationContext(), ScreenStateReceiver.this, filter);
                },
                0);
    }

    private void unregister() {
        // Unregister the receiver on the same task runner to ensure ordering.
        mTaskRunner.postDelayedTask(
                () -> {
                    ContextUtils.getApplicationContext()
                            .unregisterReceiver(ScreenStateReceiver.this);
                },
                0);
    }

    /** Resets the singleton instance of the {@link ScreenStateReceiver}. */
    public static void resetForTesting() {
        if (sInstance == null) return;

        sInstance.unregister();
        sInstance = null;
    }
}
