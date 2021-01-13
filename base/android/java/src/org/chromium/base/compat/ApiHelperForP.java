// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.compat;

import android.annotation.TargetApi;
import android.content.pm.PackageInfo;
import android.location.LocationManager;
import android.net.LinkProperties;
import android.os.Build;

import org.chromium.base.annotations.VerifiesOnP;

/**
 * Utility class to use new APIs that were added in P (API level 28). These need to exist in a
 * separate class so that Android framework can successfully verify classes without
 * encountering the new APIs.
 */
@VerifiesOnP
@TargetApi(Build.VERSION_CODES.P)
public final class ApiHelperForP {
    private ApiHelperForP() {}

    /** See {@link LinkProperties#isPrivateDnsActive() }. */
    public static boolean isPrivateDnsActive(LinkProperties linkProperties) {
        return linkProperties.isPrivateDnsActive();
    }

    /** See {@link LinkProperties#getPrivateDnsServerName() }. */
    public static String getPrivateDnsServerName(LinkProperties linkProperties) {
        return linkProperties.getPrivateDnsServerName();
    }

    /** See {@link PackageInfo#getLongVersionCode() }. */
    public static long getLongVersionCode(PackageInfo packageInfo) {
        return packageInfo.getLongVersionCode();
    }

    /** See {@link LocationManager#isLocationEnabled() }. */
    public static boolean isLocationEnabled(LocationManager locationManager) {
        return locationManager.isLocationEnabled();
    }
}
