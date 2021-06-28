// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.Manifest.permission;
import android.content.pm.PackageManager;
import android.util.Pair;

import androidx.annotation.NonNull;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.components.permissions.AndroidPermissionRequester;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.base.PermissionCallback;

/**
 * Handles file access permission requests.
 * TODO(shaktisahu): Move this to a generic location, preferably ui/android.
 */
public class FileAccessPermissionHelper {
    /**
     * Requests the storage permission from Java.
     *
     * @param delegate The permission delegate to be used for file access request.
     * @param callback Callback to notify if the permission is granted or not.
     */
    public static void requestFileAccessPermission(
            @NonNull AndroidPermissionDelegate delegate, final Callback<Boolean> callback) {
        requestFileAccessPermissionHelper(delegate, result -> {
            boolean granted = result.first;
            String permissions = result.second;
            if (granted || permissions == null) {
                callback.onResult(granted);
                return;
            }
            // TODO(jianli): When the permission request was denied by the user and "Never ask
            // again" was checked, we'd better show the permission update infobar to remind the
            // user. Currently the infobar only works for ChromeActivities. We need to investigate
            // how to make it work for other activities.
            callback.onResult(false);
        });
    }

    static void requestFileAccessPermissionHelper(@NonNull AndroidPermissionDelegate delegate,
            final Callback<Pair<Boolean, String>> callback) {
        if (delegate.hasPermission(permission.WRITE_EXTERNAL_STORAGE)) {
            callback.onResult(Pair.create(true, null));
            return;
        }

        if (!delegate.canRequestPermission(permission.WRITE_EXTERNAL_STORAGE)) {
            callback.onResult(Pair.create(false,
                    delegate.isPermissionRevokedByPolicy(permission.WRITE_EXTERNAL_STORAGE)
                            ? null
                            : permission.WRITE_EXTERNAL_STORAGE));
            return;
        }

        final AndroidPermissionDelegate permissionDelegate = delegate;
        final PermissionCallback permissionCallback = (permissions, grantResults)
                -> callback.onResult(Pair.create(grantResults.length > 0
                                && grantResults[0] == PackageManager.PERMISSION_GRANTED,
                        null));

        AndroidPermissionRequester.showMissingPermissionDialog(
                ApplicationStatus.getLastTrackedFocusedActivity(),
                org.chromium.chrome.R.string.missing_storage_permission_download_education_text,
                ()
                        -> permissionDelegate.requestPermissions(
                                new String[] {permission.WRITE_EXTERNAL_STORAGE},
                                permissionCallback),
                callback.bind(Pair.create(false, null)));
    }
}
