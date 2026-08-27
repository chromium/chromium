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
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.PermissionCallback;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * Triggers the Android runtime permission prompt UI to request missing Chrome app-level
 * permission(s) needed by the current website which already has the website-level permission, and
 * after the user expressed interest in fixing the situation in the permission update
 * infobar/message ui.
 */
@NullMarked
class PermissionUpdateRequester implements PermissionCallback {
    private final WebContents mWebContents;
    private final Set<String> mRequiredAndroidPermissions;
    private final String[] mAndroidPermisisons;
    private long mNativePtr;
    private @Nullable ActivityStateListener mActivityStateListener;

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

        mRequiredAndroidPermissions = new HashSet<>();
        Collections.addAll(mRequiredAndroidPermissions, requiredPermissions);

        Set<String> allPermissions = new HashSet<>();
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

        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            PermissionUpdateRequesterJni.get().onPermissionResult(mNativePtr, false);
            return;
        }

        if (mActivityStateListener != null) {
            ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
            mActivityStateListener = null;
        }

        // Non-tab WebContents (e.g. side-panel / bottom-sheet) may not receive
        // onRequestPermissionsResult directly, so listen for activity resume as well.
        mActivityStateListener =
                new ActivityStateListener() {
                    @Override
                    public void onActivityStateChange(Activity activity, int newState) {
                        if (newState == ActivityState.DESTROYED) {
                            if (mActivityStateListener != null) {
                                ApplicationStatus.unregisterActivityStateListener(this);
                                mActivityStateListener = null;
                            }
                            if (mNativePtr != 0) {
                                long ptr = mNativePtr;
                                mNativePtr = 0;
                                PermissionUpdateRequesterJni.get().onPermissionResult(ptr, false);
                            }
                        } else if (newState == ActivityState.RESUMED) {
                            if (mActivityStateListener != null) {
                                ApplicationStatus.unregisterActivityStateListener(this);
                                mActivityStateListener = null;
                            }
                            PostTask.postTask(
                                    TaskTraits.UI_DEFAULT,
                                    PermissionUpdateRequester.this::notifyPermissionResult);
                        }
                    }
                };
        ApplicationStatus.registerStateListenerForActivity(mActivityStateListener, activity);

        boolean canRequestAllPermissions = true;
        for (String permission : mAndroidPermisisons) {
            canRequestAllPermissions &=
                    (windowAndroid.hasPermission(permission)
                            || windowAndroid.canRequestPermission(permission));
        }

        if (canRequestAllPermissions) {
            windowAndroid.requestPermissions(mAndroidPermisisons, this);
        } else {
            Intent settingsIntent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
            settingsIntent.setData(
                    Uri.parse("package:" + ContextUtils.getApplicationContext().getPackageName()));
            settingsIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            activity.startActivity(settingsIntent);
        }
    }

    @Override
    public void onRequestPermissionsResult(String[] permissions, int[] grantResults) {
        if (mActivityStateListener != null) {
            ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
            mActivityStateListener = null;
        }
        notifyPermissionResult();
    }

    private void notifyPermissionResult() {
        boolean hasAllPermissions = true;
        WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();
        if (windowAndroid == null) {
            hasAllPermissions = false;
        } else {
            for (String permission : mAndroidPermisisons) {
                if (!mRequiredAndroidPermissions.contains(permission)) {
                    continue;
                }
                hasAllPermissions &= windowAndroid.hasPermission(permission);
            }
        }
        if (mNativePtr != 0) {
            long ptr = mNativePtr;
            mNativePtr = 0;
            PermissionUpdateRequesterJni.get().onPermissionResult(ptr, hasAllPermissions);
        }
    }

    @NativeMethods
    interface Natives {
        void onPermissionResult(
                long nativePermissionUpdateRequester, boolean allPermissionsGranted);
    }
}
