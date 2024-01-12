// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.version_info;

/**
 * A utility class for querying information about the current Chromium build. Intentionally doesn't
 * depend on native so that the data can be accessed before libchrome.so is loaded.
 */
public class VersionInfo {
    /**
     * @return Whether this build is a local build.
     */
    public static boolean isLocalBuild() {
        return VersionConstants.CHANNEL == Channel.DEFAULT;
    }

    /**
     * @return Whether this build is a canary build.
     */
    public static boolean isCanaryBuild() {
        return VersionConstants.CHANNEL == Channel.CANARY;
    }

    /**
     * @return Whether this build is a dev build.
     */
    public static boolean isDevBuild() {
        return VersionConstants.CHANNEL == Channel.DEV;
    }

    /**
     * @return Whether this build is a beta build.
     */
    public static boolean isBetaBuild() {
        return VersionConstants.CHANNEL == Channel.BETA;
    }

    /**
     * @return Whether this build is a stable build.
     */
    public static boolean isStableBuild() {
        return VersionConstants.CHANNEL == Channel.STABLE;
    }

    /**
     * @return Whether this is an official (i.e. non-development) build.
     */
    public static boolean isOfficialBuild() {
        return VersionConstants.IS_OFFICIAL_BUILD;
    }

    /**
     * @return The version number.
     */
    public static String getProductVersion() {
        return VersionConstants.PRODUCT_VERSION;
    }

    /**
     * @return The major version number.
     */
    public static int getProductMajorVersion() {
        return VersionConstants.PRODUCT_MAJOR_VERSION;
    }

    /**
     * @return The build number.
     */
    public static int getBuildVersion() {
        return VersionConstants.PRODUCT_BUILD_VERSION;
    }
}
