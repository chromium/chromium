// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.version_info;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A utility class for querying information about the current Chromium build. Intentionally doesn't
 * depend on native so that the data can be accessed before libchrome.so is loaded.
 */
@NullMarked
public class VersionInfo {
    private static @Nullable Boolean sIsOfficialBuildForTesting;
    private static @Nullable Boolean sIsStableBuildForTesting;
    private static @Nullable Boolean sIsLocalBuildForTesting;

    /**
     * @return Whether this build is a local build.
     */
    public static boolean isLocalBuild() {
        if (sIsLocalBuildForTesting != null) return sIsLocalBuildForTesting;
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
        if (sIsStableBuildForTesting != null) return sIsStableBuildForTesting;
        return VersionConstants.CHANNEL == Channel.STABLE;
    }

    /**
     * Returns a string equivalent of the current channel, independent of whether the build
     * is branded or not and without any additional modifiers.
     * @return The channel string.
     */
    public static String getChannelString() {
        // This is called by internal clients to get info about the Chrome build channel.
        return switch (VersionConstants.CHANNEL) {
            case Channel.STABLE -> "stable";
            case Channel.BETA -> "beta";
            case Channel.DEV -> "dev";
            case Channel.CANARY -> "canary";
            default -> "default";
        };
    }

    /**
     * @return Whether this is an official (i.e. non-development) build.
     */
    public static boolean isOfficialBuild() {
        if (sIsOfficialBuildForTesting != null) return sIsOfficialBuildForTesting;
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

    public static void setOverridesForTesting(Boolean official, Boolean stable, Boolean local) {
        sIsOfficialBuildForTesting = official;
        sIsStableBuildForTesting = stable;
        sIsLocalBuildForTesting = local;
        ResettersForTesting.register(
                () -> {
                    sIsOfficialBuildForTesting = null;
                    sIsStableBuildForTesting = null;
                    sIsLocalBuildForTesting = null;
                });
    }
}
