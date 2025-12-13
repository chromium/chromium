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
 * A {@link BroadcastReceiver} that listens for the screen off broadcast.
 *
 * <p>This class is a singleton that is never destroyed as it is an application level broadcast
 * receiver. Care should be taken to avoid memory leaks by removing listeners when they are no
 * longer needed.
 */
@NullMarked
public class ScreenOffBroadcastReceiver extends BroadcastReceiver {
    /** A listener for the screen off broadcast. */
    @FunctionalInterface
    public interface ScreenOffListener {
        void onScreenOff(Context context, Intent intent);
    }

    // Initialize lazily to facilitate testing.
    private static @Nullable ScreenOffBroadcastReceiver sInstance;

    private final ObserverList<ScreenOffListener> mListeners = new ObserverList<>();
    private final TaskRunner mTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.USER_VISIBLE);

    private ScreenOffBroadcastReceiver() {}

    /**
     * Adds a listener for the screen off broadcast.
     *
     * @param listener The listener to add.
     */
    public static void addListener(ScreenOffListener listener) {
        getInstance().mListeners.addObserver(listener);
    }

    /**
     * Removes a listener for the screen off broadcast.
     *
     * @param listener The listener to remove.
     */
    public static void removeListener(ScreenOffListener listener) {
        getInstance().mListeners.removeObserver(listener);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        ThreadUtils.assertOnUiThread();
        if (Intent.ACTION_SCREEN_OFF.equals(intent.getAction())) {
            for (ScreenOffListener listener : mListeners) {
                listener.onScreenOff(context, intent);
            }
        }
    }

    /** Returns the singleton instance of the {@link ScreenOffBroadcastReceiver}. */
    @VisibleForTesting
    public static ScreenOffBroadcastReceiver getInstance() {
        if (sInstance == null) {
            sInstance = new ScreenOffBroadcastReceiver();
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
                    ContextUtils.registerProtectedBroadcastReceiver(
                            ContextUtils.getApplicationContext(),
                            ScreenOffBroadcastReceiver.this,
                            filter);
                },
                0);
    }

    private void unregister() {
        // Unregister the receiver on the same task runner to ensure ordering.
        mTaskRunner.postDelayedTask(
                () -> {
                    ContextUtils.getApplicationContext()
                            .unregisterReceiver(ScreenOffBroadcastReceiver.this);
                },
                0);
    }

    /** Resets the singleton instance of the {@link ScreenOffBroadcastReceiver}. */
    public static void resetForTesting() {
        if (sInstance == null) return;

        sInstance.unregister();
        sInstance = null;
    }
}
