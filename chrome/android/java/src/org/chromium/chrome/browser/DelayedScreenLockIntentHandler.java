// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Handler;

import org.chromium.base.ContextUtils;
import org.chromium.base.JavaExceptionReporter;

import java.lang.ref.WeakReference;

/**
 * Intent handler that is specific for the situation when the screen is unlocked from pin, pattern,
 * or password.
 * This class handles exactly one intent unlock + dispatch at a time. It could be reused by calling
 * updateDeferredIntent with a new intent.
 */
public class DelayedScreenLockIntentHandler extends BroadcastReceiver {
    private static final int VALID_DEFERRED_PERIOD_MS = 10000;

    private final Handler mTaskHandler = new Handler();
    private final Runnable mUnregisterTask = () -> updateDeferredIntent(null);
    // Must be an Activity context for deferred intents without FLAG_NEW_TASK on Android P+.
    // http://crbug.com/1034440
    private final WeakReference<Activity> mActivity;
    private Intent mDeferredIntent;
    private boolean mEnabled;

    public DelayedScreenLockIntentHandler(Activity activity) {
        // Use a WeakReference so that Activity is not retained by call to postDelayed.
        mActivity = new WeakReference<>(activity);
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        assert Intent.ACTION_USER_PRESENT.equals(intent.getAction());

        if (Intent.ACTION_USER_PRESENT.equals(intent.getAction()) && mDeferredIntent != null) {
            Activity activity = mActivity.get();
            if (activity != null) {
                try {
                    activity.startActivity(mDeferredIntent);
                } catch (ActivityNotFoundException e) {
                    // TODO(crbug.com/1099819): Figure out why this happens and fix properly.
                    JavaExceptionReporter.reportException(e);
                }
            }
            // Prevent the broadcast receiver from firing intent unexpectedly.
            updateDeferredIntent(null);
        }
    }

    /**
     * Update the deferred intent with the target intent, also reset the deferred intent's lifecycle
     * @param intent Target intent
     */
    public void updateDeferredIntent(Intent intent) {
        mTaskHandler.removeCallbacks(mUnregisterTask);
        mTaskHandler.postDelayed(mUnregisterTask, VALID_DEFERRED_PERIOD_MS);
        mDeferredIntent = intent;

        setEnabled(intent != null);
    }

    /**
     * Unregister the receiver in one of the following situations
     * - When the deferred intent expires
     * - When updateDeferredIntent(null) called
     * - When the deferred intent has been fired
     * Register to receive ACTION_USER_PRESENT when the screen is unlocked.
     * The ACTION_USER_PRESENT is sent by platform to indicates when user is present.
     */
    private void setEnabled(boolean value) {
        if (value == mEnabled) return;
        mEnabled = value;
        Context applicationContext = ContextUtils.getApplicationContext();
        if (value) {
            ContextUtils.registerProtectedBroadcastReceiver(
                    applicationContext, this, new IntentFilter(Intent.ACTION_USER_PRESENT));
        } else {
            applicationContext.unregisterReceiver(this);
        }
    }
}
