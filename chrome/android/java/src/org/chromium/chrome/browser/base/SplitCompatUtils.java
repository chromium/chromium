// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import static org.chromium.chrome.browser.base.SplitCompatApplication.CHROME_SPLIT_NAME;

import android.content.Context;

import org.chromium.base.BundleUtils;
import org.chromium.build.annotations.NullMarked;

/** Helper functions for SplitCompat classes. */
@NullMarked
public class SplitCompatUtils {

    private SplitCompatUtils() {}

    public static Object loadClassAndAdjustContextChrome(Context context, String className) {
        return loadClassAndAdjustContext(context, className, CHROME_SPLIT_NAME);
    }

    public static Object loadClassAndAdjustContext(
            Context context, String className, String splitName) {
        BundleUtils.replaceClassLoader(context, BundleUtils.getOrCreateSplitClassLoader(splitName));
        return BundleUtils.newInstance(className, splitName);
    }
}
