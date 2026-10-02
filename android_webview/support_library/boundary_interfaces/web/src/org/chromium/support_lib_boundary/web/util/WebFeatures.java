// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary.web.util;

import android.os.Build;

import org.jspecify.annotations.NullMarked;

import java.util.Arrays;
import java.util.Collection;

/**
 * Class containing all the features the AndroidX Web library can support. This class lives in the
 * boundary interface directory so that the Android Support Library and Chromium can share its
 * definition.
 */
@NullMarked
public final class WebFeatures {

    // Features suffixed with DEV will only be visible on debug devices.
    public static final String DEV_SUFFIX = ":dev";

    // This class contains constants representing features and the APIs to check for them.
    private WebFeatures() {}

    // WebContentBuilder
    public static final String WEB_CONTENT = "WEB_CONTENT";

    // WebFeature.WEB_SURFACE
    public static final String WEB_SURFACE = "WEB_SURFACE";

    /**
     * Check if this is a debuggable build of Android. Note: we copy ApkInfo's method because we
     * cannot depend on the base-layer here (this folder is mirrored into Android).
     */
    private static boolean isDebuggable() {
        return "eng".equals(Build.TYPE) || "userdebug".equals(Build.TYPE);
    }

    /**
     * Check whether a set of features {@code features} contains a certain feature {@code
     * soughtFeature}.
     */
    public static boolean containsFeature(Collection<String> features, String soughtFeature) {
        assert !soughtFeature.endsWith(DEV_SUFFIX);
        return features.contains(soughtFeature)
                || (isDebuggable() && features.contains(soughtFeature + DEV_SUFFIX));
    }

    public static boolean containsFeature(String[] features, String soughtFeature) {
        return containsFeature(Arrays.asList(features), soughtFeature);
    }
}
