// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.text.TextUtils;

import org.chromium.base.PackageUtils;

import java.util.Map;

/** Grabs a list of all granted permissions. */
class PermissionFeedbackSource implements FeedbackSource {
    PermissionFeedbackSource() {}

    @Override
    public Map<String, String> getFeedback() {
        String grantedPermissions = "";
        String notGrantedPermission = "";

        PackageInfo pi = PackageUtils.getApplicationPackageInfo(PackageManager.GET_PERMISSIONS);
        if (pi == null || pi.requestedPermissions == null) return null;

        for (int i = 0; i < pi.requestedPermissions.length; i++) {
            int flags = pi.requestedPermissionsFlags[i];
            String permission = pi.requestedPermissions[i];
            if ((flags & PackageInfo.REQUESTED_PERMISSION_GRANTED) != 0) {
                if (!TextUtils.isEmpty(grantedPermissions)) grantedPermissions += ", ";
                grantedPermissions += permission;
            } else {
                if (!TextUtils.isEmpty(notGrantedPermission)) notGrantedPermission += ", ";
                notGrantedPermission += permission;
            }
        }

        return Map.of(
                "Granted Permissions",
                grantedPermissions,
                "Not Granted or Requested Permissions",
                notGrantedPermission);
    }
}
