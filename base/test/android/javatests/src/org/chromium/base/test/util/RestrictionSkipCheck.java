// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.text.TextUtils;

import org.junit.runners.model.FrameworkMethod;

import org.chromium.base.Log;
import org.chromium.base.SysUtils;

/**
 * Checks if any restrictions exist and skip the test if it meets those restrictions.
 */
public class RestrictionSkipCheck extends SkipCheck {

    private static final String TAG = "base_test";

    private final Context mTargetContext;

    public RestrictionSkipCheck(Context targetContext) {
        mTargetContext = targetContext;
    }

    protected Context getTargetContext() {
        return mTargetContext;
    }

    @Override
    public boolean shouldSkip(FrameworkMethod frameworkMethod) {
        if (frameworkMethod == null) return true;

        for (Restriction restriction : AnnotationProcessingUtils.getAnnotations(
                     frameworkMethod.getMethod(), Restriction.class)) {
            for (String restrictionVal : restriction.value()) {
                if (restrictionApplies(restrictionVal)) {
                    Log.i(TAG, "Test " + frameworkMethod.getDeclaringClass().getName() + "#"
                            + frameworkMethod.getName() + " skipped because of restriction "
                            + restriction);
                    return true;
                }
            }
        }
        return false;
    }

    protected boolean restrictionApplies(String restriction) {
        if (TextUtils.equals(restriction, Restriction.RESTRICTION_TYPE_LOW_END_DEVICE)
                && !SysUtils.isLowEndDevice()) {
            return true;
        }
        if (TextUtils.equals(restriction, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
                && SysUtils.isLowEndDevice()) {
            return true;
        }
        if (TextUtils.equals(restriction, Restriction.RESTRICTION_TYPE_INTERNET)
                && !isNetworkAvailable()) {
            return true;
        }
        if (TextUtils.equals(restriction, Restriction.RESTRICTION_TYPE_HAS_CAMERA)
                && !SysUtils.hasCamera(mTargetContext)) {
            return true;
        }
        return false;
    }

    private boolean isNetworkAvailable() {
        final ConnectivityManager connectivityManager = (ConnectivityManager)
                mTargetContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        final NetworkInfo activeNetworkInfo = connectivityManager.getActiveNetworkInfo();
        return activeNetworkInfo != null && activeNetworkInfo.isConnected();
    }
}
