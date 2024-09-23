// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;

import androidx.test.platform.app.InstrumentationRegistry;

import org.chromium.base.SysUtils;

/** Restriction handlers for restrictions in Restrictions. */
public class BaseRestrictions {
    private static Boolean sNetworkAvailable;
    private static Boolean sHasCamera;

    private static boolean isNetworkAvailable() {
        if (sNetworkAvailable == null) {
            Context targetContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
            final ConnectivityManager connectivityManager =
                    (ConnectivityManager)
                            targetContext.getSystemService(Context.CONNECTIVITY_SERVICE);
            final NetworkInfo activeNetworkInfo = connectivityManager.getActiveNetworkInfo();
            sNetworkAvailable = activeNetworkInfo != null && activeNetworkInfo.isConnected();
        }
        return sNetworkAvailable;
    }

    private static boolean hasCamera() {
        if (sHasCamera == null) {
            Context targetContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
            sHasCamera = SysUtils.hasCamera(targetContext);
        }
        return sHasCamera;
    }

    public static void registerChecks(RestrictionSkipCheck restrictionSkipCheck) {
        restrictionSkipCheck.addHandler(
                Restriction.RESTRICTION_TYPE_LOW_END_DEVICE, () -> !SysUtils.isLowEndDevice());
        restrictionSkipCheck.addHandler(
                Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE, SysUtils::isLowEndDevice);
        restrictionSkipCheck.addHandler(
                Restriction.RESTRICTION_TYPE_INTERNET, () -> !isNetworkAvailable());
        restrictionSkipCheck.addHandler(
                Restriction.RESTRICTION_TYPE_HAS_CAMERA, () -> !hasCamera());
    }
}
