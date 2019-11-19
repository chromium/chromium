// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;

import android.support.test.filters.SmallTest;

import com.google.android.libraries.feed.api.host.config.ApplicationInfo;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.BuildInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.version_info.Channel;
import org.chromium.components.version_info.VersionConstants;

/** Unit tests for {@link FeedApplicationInfo}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedApplicationInfoTest {
    @Test
    @SmallTest
    public void testGetArchitecture() {
        assertEquals(
                ApplicationInfo.Architecture.MIPS, FeedApplicationInfo.getArchitecture("mips"));
        assertEquals(
                ApplicationInfo.Architecture.MIPS64, FeedApplicationInfo.getArchitecture("mips64"));
        assertEquals(
                ApplicationInfo.Architecture.ARM, FeedApplicationInfo.getArchitecture("armeabi"));
        assertEquals(ApplicationInfo.Architecture.ARM,
                FeedApplicationInfo.getArchitecture("armeabi-v7a"));
        assertEquals(ApplicationInfo.Architecture.ARM64,
                FeedApplicationInfo.getArchitecture("arm64-v8a"));
        assertEquals(ApplicationInfo.Architecture.X86, FeedApplicationInfo.getArchitecture("x86"));
        assertEquals(
                ApplicationInfo.Architecture.X86_64, FeedApplicationInfo.getArchitecture("x86_64"));
        assertEquals(ApplicationInfo.Architecture.UNKNOWN_ACHITECTURE,
                FeedApplicationInfo.getArchitecture("notarealthing"));
    }

    @Test
    @SmallTest
    public void testGetBuildType() {
        assertEquals(ApplicationInfo.BuildType.RELEASE,
                FeedApplicationInfo.getBuildType(Channel.STABLE));
        assertEquals(
                ApplicationInfo.BuildType.BETA, FeedApplicationInfo.getBuildType(Channel.BETA));
        assertEquals(
                ApplicationInfo.BuildType.ALPHA, FeedApplicationInfo.getBuildType(Channel.DEV));
        assertEquals(
                ApplicationInfo.BuildType.DEV, FeedApplicationInfo.getBuildType(Channel.CANARY));
        assertEquals(ApplicationInfo.BuildType.UNKNOWN_BUILD_TYPE,
                FeedApplicationInfo.getBuildType(Channel.DEFAULT));
        assertEquals(ApplicationInfo.BuildType.UNKNOWN_BUILD_TYPE,
                FeedApplicationInfo.getBuildType(Channel.UNKNOWN));
    }

    @Test
    @SmallTest
    public void testcreateApplicationInfo() {
        ApplicationInfo info = FeedApplicationInfo.createApplicationInfo();
        assertEquals(ApplicationInfo.AppType.CHROME, info.getAppType());
        assertEquals(FeedApplicationInfo.getArchitecture(BuildInfo.getInstance().abiString),
                info.getArchitecture());
        assertEquals(
                FeedApplicationInfo.getBuildType(VersionConstants.CHANNEL), info.getBuildType());
        // Don't test version, it isn't set correctly for unit tests.
    }
}
