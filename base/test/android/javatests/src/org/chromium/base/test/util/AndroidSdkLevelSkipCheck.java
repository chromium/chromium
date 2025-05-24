// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.os.Build;

import org.junit.runners.model.FrameworkMethod;

import org.chromium.base.Log;

/** Checks the device's SDK level against any specified minimum or maximum requirement. */
public class AndroidSdkLevelSkipCheck extends SkipCheck {
    private static final String TAG = "base_test";

    /**
     * If either {@link MinAndroidSdkLevel} or {@link MaxAndroidSdkLevel} is present, checks its
     * boundary against the device's SDK level.
     *
     * @return true if the device's SDK level is below the specified minimum.
     */
    @Override
    public boolean shouldSkip(FrameworkMethod frameworkMethod) {
        int minSdkLevel = 0;
        for (MinAndroidSdkLevel m :
                AnnotationProcessingUtils.getAnnotations(
                        frameworkMethod.getMethod(), MinAndroidSdkLevel.class)) {
            minSdkLevel = Math.max(minSdkLevel, m.value());
        }
        int maxSdkLevel = Integer.MAX_VALUE;
        for (MaxAndroidSdkLevel m :
                AnnotationProcessingUtils.getAnnotations(
                        frameworkMethod.getMethod(), MaxAndroidSdkLevel.class)) {
            maxSdkLevel = Math.min(maxSdkLevel, m.value());
        }
        if (Build.VERSION.SDK_INT < minSdkLevel || Build.VERSION.SDK_INT > maxSdkLevel) {
            Log.i(
                    TAG,
                    "Test "
                            + frameworkMethod.getDeclaringClass().getName()
                            + "#"
                            + frameworkMethod.getName()
                            + " is not enabled at SDK level "
                            + Build.VERSION.SDK_INT
                            + ".");
            return true;
        }
        return false;
    }
}
