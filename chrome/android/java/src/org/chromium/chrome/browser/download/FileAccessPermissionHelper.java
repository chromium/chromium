// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.Manifest.permission;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.Pair;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.components.permissions.AndroidPermissionRequester;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionCallback;

import java.util.function.Consumer;

/**
 * Handles file access permission requests.
 * TODO(shaktisahu): Move this to a generic location, preferably ui/android.
 */
public class FileAccessPermissionHelper {
    /**
     * Requests the storage permission from Java.
     *
     * @param windowAndroid The window to be used for file access request.
     * @param callback Callback to notify if the permission is granted or not.
     */
    public static void requestFileAccessPermission(
            @NonNull WindowAndroid windowAndroid, final Callback<Boolean> callback) {
        requestFileAccessPermissionHelper(
                windowAndroid,
                result -> {
                    boolean granted = result.first;
                    String permissions = result.second;
                    if (granted || permissions == null) {
                        callback.onResult(granted);
                        return;
                    }
                    // TODO(jianli): When the permission request was denied by the user and "Never
                    // ask again" was checked, we'd better show the permission update infobar to
                    // remind the user. Currently the infobar only works for ChromeActivities. We
                    // need to investigate how to make it work for other activities.
                    callback.onResult(false);
                });
    }

    static void requestFileAccessPermissionHelper(
            @NonNull WindowAndroid windowAndroid, final Callback<Pair<Boolean, String>> callback) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
                || windowAndroid.hasPermission(permission.WRITE_EXTERNAL_STORAGE)) {
            callback.onResult(Pair.create(true, null));
            return;
        }

        if (!windowAndroid.canRequestPermission(permission.WRITE_EXTERNAL_STORAGE)) {
            callback.onResult(
                    Pair.create(
                            false,
                            windowAndroid.isPermissionRevokedByPolicy(
                                            permission.WRITE_EXTERNAL_STORAGE)
                                    ? null
                                    : permission.WRITE_EXTERNAL_STORAGE));
            return;
        }

        final AndroidPermissionDelegate permissionDelegate = windowAndroid;

        Context context = windowAndroid.getContext().get();
        if (context == null) {
            callback.onResult(Pair.create(false, null));
            return;
        }

        Consumer<PropertyModel> requestPermissions =
                (model) -> {
                    PermissionCallback permissionCallback =
                            (permissions, grantResults) -> {
                                final ModalDialogManager modalDialogManager =
                                        windowAndroid.getModalDialogManager();
                                // If the model is not null, it means that it has not been dismissed
                                // yet and we will be dismissing it after the permissions
                                // callback. For more context, crbug/1319659
                                if (modalDialogManager != null && model != null) {
                                    modalDialogManager.dismissDialog(
                                            model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                                }
                                boolean granted =
                                        grantResults.length > 0
                                                && grantResults[0]
                                                        == PackageManager.PERMISSION_GRANTED;
                                callback.onResult(Pair.create(granted, null));
                            };

                    permissionDelegate.requestPermissions(
                            new String[] {permission.WRITE_EXTERNAL_STORAGE}, permissionCallback);
                };

        if (windowAndroid.getModalDialogManager() != null) {
            AndroidPermissionRequester.showMissingPermissionDialog(
                    windowAndroid,
                    context.getString(
                            org.chromium.chrome.R.string
                                    .missing_storage_permission_download_education_text),
                    requestPermissions,
                    callback.bind(Pair.create(false, null)));
        } else {
            // If there is no modal dialog manager, we will request permissions directly.
            requestPermissions.accept(null);
        }
    }
}
