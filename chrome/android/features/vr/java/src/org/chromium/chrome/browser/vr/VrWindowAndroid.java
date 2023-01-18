// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Process;

import androidx.annotation.Nullable;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionCallback;

import java.lang.ref.WeakReference;
import java.util.Arrays;

/**
 * The class provides the WindowAndroid's implementation which requires Activity Instance. It is
 * only intended to be used when in VR.
 */
public class VrWindowAndroid
        extends WindowAndroid implements ApplicationStatus.ActivityStateListener {
    /**
     * @param activity The current application {@link Activity}.
     * @param display The application {@link DisplayAndroid}.
     */
    public VrWindowAndroid(Activity activity, DisplayAndroid display) {
        super(activity, display);
        ApplicationStatus.registerStateListenerForActivity(this, activity);
        setAndroidPermissionDelegate(new ActivityAndroidPermissionDelegate());
    }

    @Override
    public int showCancelableIntent(
            PendingIntent intent, IntentCallback callback, Integer errorId) {
        return START_INTENT_FAILURE;
    }

    @Override
    public int showCancelableIntent(Intent intent, IntentCallback callback, Integer errorId) {
        return START_INTENT_FAILURE;
    }

    @Override
    public void cancelIntent(int requestCode) {}

    @Override
    public WeakReference<Activity> getActivity() {
        return new WeakReference<>(ContextUtils.activityFromContext(getContext().get()));
    }

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (newState == ActivityState.STOPPED) {
            onActivityStopped();
        } else if (newState == ActivityState.STARTED) {
            onActivityStarted();
        }
    }

    @Override
    public @Nullable ModalDialogManager getModalDialogManager() {
        return null;
    }

    // We can't request permissions inside of VR without getting kicked out of VR.
    private class ActivityAndroidPermissionDelegate implements AndroidPermissionDelegate {
        @Override
        public boolean hasPermission(String permission) {
            return ApiCompatibilityUtils.checkPermission(ContextUtils.getApplicationContext(),
                           permission, Process.myPid(), Process.myUid())
                    == PackageManager.PERMISSION_GRANTED;
        }

        @Override
        public boolean canRequestPermission(String permission) {
            return false;
        }

        @Override
        public boolean isPermissionRevokedByPolicy(String permission) {
            return false;
        }

        @Override
        public void requestPermissions(
                final String[] permissions, final PermissionCallback callback) {
            int[] grantResults = new int[permissions.length];
            Arrays.fill(grantResults, PackageManager.PERMISSION_DENIED);
            callback.onRequestPermissionsResult(permissions, grantResults);
        }

        @Override
        public boolean handlePermissionResult(
                int requestCode, String[] permissions, int[] grantResults) {
            return false;
        }
    }
}
