// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.pm.ApplicationInfo;
import android.system.Os;
import android.system.StructStat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.util.ReflectionHelpers.ClassParameter;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.IOException;

/** Unit tests for {@link DexFixer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {DexFixerTest.ShadowOs.class})
public class DexFixerTest {
    @Implements(Os.class)
    public static class ShadowOs {
        static boolean sWorldReadable = true;

        @Implementation
        public static StructStat stat(String path) {
            if (path.endsWith(".odex")) {
                return new StructStat(
                        0, 0, sWorldReadable ? 0777 : 0700, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            }
            return Shadow.directlyOn(Os.class, "stat", ClassParameter.from(String.class, path));
        }
    }

    @Mock private Runtime mMockRuntime;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() {
        ShadowOs.sWorldReadable = true;
    }

    private void verifyDexOpt() {
        try {
            verify(mMockRuntime)
                    .exec(Mockito.matches("/system/bin/cmd package compile -r shared \\S+"));
        } catch (IOException e) {
            // Mocks don't actually throw...
        }
    }

    @Test
    public void testFixDexIfNecessary_notNeeded() {
        @DexFixerReason int reason = DexFixer.fixDexIfNecessary(mMockRuntime);
        assertThat(reason).isEqualTo(DexFixerReason.NOT_NEEDED);
        verifyNoMoreInteractions(mMockRuntime);
    }

    @Test
    public void testFixDexIfNecessary_notReadable() {
        ShadowOs.sWorldReadable = false;
        @DexFixerReason int reason = DexFixer.fixDexIfNecessary(mMockRuntime);
        assertThat(reason).isEqualTo(DexFixerReason.NOT_READABLE);
        verifyDexOpt();
    }

    @Test
    public void testFixDexIfNecessary_notReadableWithSplits() {
        ApplicationInfo appInfo = ContextUtils.getApplicationContext().getApplicationInfo();
        appInfo.splitNames = new String[] {"ignored.en"};
        ShadowOs.sWorldReadable = false;

        @DexFixerReason int reason = DexFixer.fixDexIfNecessary(mMockRuntime);
        assertThat(reason).isEqualTo(DexFixerReason.NOT_READABLE);
        verifyDexOpt();
    }
}
