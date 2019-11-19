// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.view.autofill.AutofillManager;
import android.view.autofill.AutofillValue;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Iterator;

/**
 * The class to call Android's AutofillManager.
 */
@TargetApi(Build.VERSION_CODES.O)
public class AwAutofillManager {
    // Don't change TAG, it is used for runtime log.
    public static final String TAG = "AwAutofillManager";

    /**
     * The observer of suggestion window.
     */
    public static interface InputUIObserver { void onInputUIShown(); }

    private static class AutofillInputUIMonitor extends AutofillManager.AutofillCallback {
        private WeakReference<AwAutofillManager> mManager;

        public AutofillInputUIMonitor(AwAutofillManager manager) {
            mManager = new WeakReference<AwAutofillManager>(manager);
        }

        @Override
        public void onAutofillEvent(View view, int virtualId, int event) {
            AwAutofillManager manager = mManager.get();
            if (manager == null) return;
            manager.mIsAutofillInputUIShowing = (event == EVENT_INPUT_SHOWN);
            if (event == EVENT_INPUT_SHOWN) manager.notifyInputUIChange();
        }
    }

    private static boolean sIsLoggable;
    private AutofillManager mAutofillManager;
    private boolean mIsAutofillInputUIShowing;
    private AutofillInputUIMonitor mMonitor;
    private boolean mDestroyed;
    private boolean mDisabled;
    private ArrayList<WeakReference<InputUIObserver>> mInputUIObservers;

    public AwAutofillManager(Context context) {
        updateLogStat();
        if (isLoggable()) log("constructor");
        mAutofillManager = context.getSystemService(AutofillManager.class);
        mDisabled = mAutofillManager == null || !mAutofillManager.isEnabled();
        if (mDisabled) {
            if (isLoggable()) log("disabled");
            return;
        }

        mMonitor = new AutofillInputUIMonitor(this);
        mAutofillManager.registerCallback(mMonitor);
    }

    public void notifyVirtualValueChanged(View parent, int childId, AutofillValue value) {
        if (mDisabled || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("notifyVirtualValueChanged");
        mAutofillManager.notifyValueChanged(parent, childId, value);
    }

    public void commit(int submissionSource) {
        if (mDisabled || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("commit source:" + submissionSource);
        mAutofillManager.commit();
    }

    public void cancel() {
        if (mDisabled || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("cancel");
        mAutofillManager.cancel();
    }

    public void notifyVirtualViewEntered(View parent, int childId, Rect absBounds) {
        // Log warning only when the autofill is triggered.
        if (mDisabled) {
            Log.w(TAG,
                    "WebView autofill is disabled because WebView isn't created with "
                            + "activity context.");
            return;
        }
        if (checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("notifyVirtualViewEntered");
        mAutofillManager.notifyViewEntered(parent, childId, absBounds);
    }

    public void notifyVirtualViewExited(View parent, int childId) {
        if (mDisabled || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("notifyVirtualViewExited");
        mAutofillManager.notifyViewExited(parent, childId);
    }

    public void requestAutofill(View parent, int virtualId, Rect absBounds) {
        if (mDisabled || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("requestAutofill");
        mAutofillManager.requestAutofill(parent, virtualId, absBounds);
    }

    public boolean isAutofillInputUIShowing() {
        if (mDisabled || checkAndWarnIfDestroyed()) return false;
        if (isLoggable()) log("isAutofillInputUIShowing: " + mIsAutofillInputUIShowing);
        return mIsAutofillInputUIShowing;
    }

    public void destroy() {
        if (mDisabled || checkAndWarnIfDestroyed()) return;
        if (isLoggable()) log("destroy");
        mAutofillManager.unregisterCallback(mMonitor);
        mAutofillManager = null;
        mDestroyed = true;
    }

    public boolean isDisabled() {
        return mDisabled;
    }

    private boolean checkAndWarnIfDestroyed() {
        if (mDestroyed) {
            Log.w(TAG, "Application attempted to call on a destroyed AwAutofillManager",
                    new Throwable());
        }
        return mDestroyed;
    }

    public void addInputUIObserver(InputUIObserver observer) {
        if (observer == null) return;
        if (mInputUIObservers == null) {
            mInputUIObservers = new ArrayList<WeakReference<InputUIObserver>>();
        }
        mInputUIObservers.add(new WeakReference<InputUIObserver>(observer));
    }

    public void removeInputUIObserver(InputUIObserver observer) {
        if (observer == null) return;
        for (Iterator<WeakReference<InputUIObserver>> i = mInputUIObservers.listIterator();
                i.hasNext();) {
            WeakReference<InputUIObserver> o = i.next();
            if (o.get() == null || o.get() == observer) i.remove();
        }
    }

    @VisibleForTesting
    public void notifyInputUIChange() {
        for (Iterator<WeakReference<InputUIObserver>> i = mInputUIObservers.listIterator();
                i.hasNext();) {
            WeakReference<InputUIObserver> o = i.next();
            InputUIObserver observer = o.get();
            if (observer == null) {
                i.remove();
                continue;
            }
            observer.onInputUIShown();
        }
    }

    public void notifyNewSessionStarted() {
        updateLogStat();
        if (isLoggable()) log("Session starts");
    }

    /**
     * Always check isLoggable() before call this method.
     */
    public static void log(String log) {
        // Log.i() instead of Log.d() is used here because log.d() is stripped out in release build.
        Log.i(TAG, log);
    }

    public static boolean isLoggable() {
        return sIsLoggable;
    }

    private static void updateLogStat() {
        // Use 'setprop log.tag.AwAutofillManager DEBUG' to enable the log at runtime.
        sIsLoggable = Log.isLoggable(TAG, Log.DEBUG);
    }
}
