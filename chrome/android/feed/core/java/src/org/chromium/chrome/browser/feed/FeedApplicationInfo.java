// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.host.config.ApplicationInfo;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.components.version_info.Channel;
import org.chromium.components.version_info.VersionConstants;

/**
 * Logic to translate built in constants from ChromeVersionInfo into an {@link ApplicationInfo} the
 * Feed can consume.
 * */
public final class FeedApplicationInfo {
    /** Do not allow construction */
    private FeedApplicationInfo() {}

    // Constants to see if they are contained in the ABI string.
    private static final String ABI_64_SUBSTRING = "64";
    private static final String ABI_MIPS_SUBSTRING = "mips";
    private static final String ABI_ARM_SUBSTRING = "arm";
    private static final String ABI_X86_SUBSTRING = "x86";

    /**
     * @param abi The first ABI string returned by the Android API.
     * @return Device architecture in a Feed consumable format.
     */
    @VisibleForTesting
    static int getArchitecture(String abi) {
        boolean is64 = abi.contains(ABI_64_SUBSTRING);
        if (abi.contains(ABI_MIPS_SUBSTRING)) {
            return is64 ? ApplicationInfo.Architecture.MIPS64 : ApplicationInfo.Architecture.MIPS;
        } else if (abi.contains(ABI_ARM_SUBSTRING)) {
            return is64 ? ApplicationInfo.Architecture.ARM64 : ApplicationInfo.Architecture.ARM;
        } else if (abi.contains(ABI_X86_SUBSTRING)) {
            return is64 ? ApplicationInfo.Architecture.X86_64 : ApplicationInfo.Architecture.X86;
        } else {
            return ApplicationInfo.Architecture.UNKNOWN_ACHITECTURE;
        }
    }

    /**
     * @channel The channel this was build for.
     * @return Channel in a Feed consumable format.
     */
    @VisibleForTesting
    static int getBuildType(@Channel int channel) {
        switch (channel) {
            case Channel.STABLE:
                return ApplicationInfo.BuildType.RELEASE;
            case Channel.BETA:
                return ApplicationInfo.BuildType.BETA;
            case Channel.DEV:
                return ApplicationInfo.BuildType.ALPHA;
            case Channel.CANARY:
                return ApplicationInfo.BuildType.DEV;
            default:
                return ApplicationInfo.BuildType.UNKNOWN_BUILD_TYPE;
        }
    }

    /**
     * @return A fully built {@link ApplicationInfo }, ready to be given to the Feed.
     */
    public static ApplicationInfo createApplicationInfo() {
        // Don't need to set the version string, it'll pick that up correctly on its own.
        return new ApplicationInfo.Builder(ContextUtils.getApplicationContext())
                .setAppType(ApplicationInfo.AppType.CHROME)
                .setBuildType(getBuildType(VersionConstants.CHANNEL))
                .setArchitecture(getArchitecture(BuildInfo.getInstance().abiString))
                .build();
    }
}
