// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.text.TextUtils;
import android.util.Pair;

import org.chromium.base.CollectionUtil;
import org.chromium.base.ContextUtils;

import java.util.Map;

/** Grabs a list of all granted permissions. */
class PermissionFeedbackSource implements FeedbackSource {
    PermissionFeedbackSource() {}

    @Override
    public Map<String, String> getFeedback() {
        String grantedPermissions = "";
        String notGrantedPermission = "";

        try {
            Context ctx = ContextUtils.getApplicationContext();
            PackageInfo pi = ctx.getPackageManager().getPackageInfo(
                    ctx.getPackageName(), PackageManager.GET_PERMISSIONS);
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
        } catch (NameNotFoundException e) {
            return null;
        }

        return CollectionUtil.newHashMap(Pair.create("Granted Permissions", grantedPermissions),
                Pair.create("Not Granted or Requested Permissions", notGrantedPermission));
    }
}