// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.provider.Settings;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ContextUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.PermissionCallback;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * Triggers the Android runtime permission prompt UI to request missing
 * Chrome app-level permission(s) needed by the current website which already has the
 * website-level permission, and after the user expressed interest in fixing the situation
 * in the permission update infobar/message ui.
 */
class PermissionUpdateRequester implements PermissionCallback {
    private final WebContents mWebContents;
    private final Set<String> mRequiredAndroidPermissions;
    private final String[] mAndroidPermisisons;
    private long mNativePtr;
    private ActivityStateListener mActivityStateListener;

    @CalledByNative
    private static PermissionUpdateRequester create(
            long nativePtr,
            WebContents webContents,
            String[] requiredPermissions,
            String[] optionalPermissions) {
        return new PermissionUpdateRequester(
                nativePtr, webContents, requiredPermissions, optionalPermissions);
    }

    private PermissionUpdateRequester(
            long nativePtr,
            WebContents webContents,
            String[] requiredPermissions,
            String[] optionalPermissions) {
        mNativePtr = nativePtr;
        mWebContents = webContents;

        mRequiredAndroidPermissions = new HashSet<String>();
        Collections.addAll(mRequiredAndroidPermissions, requiredPermissions);

        Set<String> allPermissions = new HashSet<String>();
        Collections.addAll(allPermissions, requiredPermissions);
        Collections.addAll(allPermissions, optionalPermissions);
        mAndroidPermisisons = allPermissions.toArray(new String[allPermissions.size()]);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativePtr = 0;
        if (mActivityStateListener != null) {
            ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
            mActivityStateListener = null;
        }
    }

    @CalledByNative
    private void requestPermissions() {
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        if (windowAndroid == null) {
            PermissionUpdateRequesterJni.get().onPermissionResult(mNativePtr, false);
            return;
        }

        boolean canRequestAllPermissions = true;
        for (int i = 0; i < mAndroidPermisisons.length; i++) {
            canRequestAllPermissions &=
                    (windowAndroid.hasPermission(mAndroidPermisisons[i])
                            || windowAndroid.canRequestPermission(mAndroidPermisisons[i]));
        }

        Activity activity = windowAndroid.getActivity().get();
        if (canRequestAllPermissions) {
            windowAndroid.requestPermissions(mAndroidPermisisons, this);
        } else {
            if (activity == null) {
                PermissionUpdateRequesterJni.get().onPermissionResult(mNativePtr, false);
                return;
            }

            mActivityStateListener =
                    new ActivityStateListener() {
                        @Override
                        public void onActivityStateChange(Activity activity, int newState) {
                            if (newState == ActivityState.DESTROYED) {
                                ApplicationStatus.unregisterActivityStateListener(this);
                                mActivityStateListener = null;

                                PermissionUpdateRequesterJni.get()
                                        .onPermissionResult(mNativePtr, false);
                            } else if (newState == ActivityState.RESUMED) {
                                ApplicationStatus.unregisterActivityStateListener(this);
                                mActivityStateListener = null;

                                notifyPermissionResult();
                            }
                        }
                    };
            ApplicationStatus.registerStateListenerForActivity(mActivityStateListener, activity);

            Intent settingsIntent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
            settingsIntent.setData(
                    Uri.parse("package:" + ContextUtils.getApplicationContext().getPackageName()));
            settingsIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            activity.startActivity(settingsIntent);
        }
    }

    @Override
    public void onRequestPermissionsResult(String[] permissions, int[] grantResults) {
        notifyPermissionResult();
    }

    private void notifyPermissionResult() {
        boolean hasAllPermissions = true;
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        if (windowAndroid == null) {
            hasAllPermissions = false;
        } else {
            for (int i = 0; i < mAndroidPermisisons.length; i++) {
                if (!mRequiredAndroidPermissions.contains(mAndroidPermisisons[i])) {
                    continue;
                }
                hasAllPermissions &= windowAndroid.hasPermission(mAndroidPermisisons[i]);
            }
        }
        if (mNativePtr != 0) {
            PermissionUpdateRequesterJni.get().onPermissionResult(mNativePtr, hasAllPermissions);
        }
    }

    @NativeMethods
    interface Natives {
        void onPermissionResult(
                long nativePermissionUpdateRequester, boolean allPermissionsGranted);
    }
}
