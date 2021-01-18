// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.base;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.pm.ApplicationInfo;
import android.os.Build;
import android.system.Os;
import android.system.StructStat;

import org.junit.After;
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
import org.robolectric.shadows.ShadowDexFile;
import org.robolectric.util.ReflectionHelpers.ClassParameter;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

import java.io.IOException;

/**
 * Unit tests for {@link DexFixer}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.O_MR1,
        shadows = {ShadowRecordHistogram.class, DexFixerTest.ShadowOs.class})
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

    @Mock
    private Runtime mMockRuntime;
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() {
        ShadowOs.sWorldReadable = true;
    }

    @After
    public void tearDown() {
        ShadowRecordHistogram.reset();
        DexFixer.setHasIsolatedSplits(false);
    }

    private static int getReason() {
        int ret = -1;
        for (int i = 0; i < DexFixerReason.COUNT; ++i) {
            int count =
                    ShadowRecordHistogram.getHistogramValueCountForTesting("Android.DexFixer", i);
            if (count > 0) {
                assertThat(count).isEqualTo(1);
                assertThat(ret).isEqualTo(-1);
                ret = i;
            }
        }
        assertThat(ret).isNotEqualTo(-1);
        return ret;
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
        DexFixer.fixDexIfNecessary(mMockRuntime);
        assertThat(getReason()).isEqualTo(DexFixerReason.NOT_NEEDED);
        verifyNoMoreInteractions(mMockRuntime);
    }

    @Test
    public void testFixDexIfNecessary_notReadable() {
        ShadowOs.sWorldReadable = false;
        DexFixer.fixDexIfNecessary(mMockRuntime);
        assertThat(getReason()).isEqualTo(DexFixerReason.NOT_READABLE);
        verifyDexOpt();
    }

    @Test
    public void testFixDexIfNecessary_update() {
        DexFixer.setHasIsolatedSplits(true);
        DexFixer.fixDexIfNecessary(mMockRuntime);
        assertThat(getReason()).isEqualTo(DexFixerReason.O_MR1_AFTER_UPDATE);
        verifyDexOpt();

        // Second time should be okay.
        ShadowRecordHistogram.reset();
        DexFixer.fixDexIfNecessary(mMockRuntime);
        assertThat(getReason()).isEqualTo(DexFixerReason.NOT_NEEDED);
        verifyNoMoreInteractions(mMockRuntime);
    }

    @Test
    public void testFixDexIfNecessary_corruptDex() {
        ApplicationInfo appInfo = ContextUtils.getApplicationContext().getApplicationInfo();
        appInfo.splitNames = new String[] {"a"};
        appInfo.splitSourceDirs = new String[] {"/a.apk"};
        DexFixer.setHasIsolatedSplits(true);
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.ISOLATED_SPLITS_DEX_COMPILE_VERSION,
                BuildInfo.getInstance().versionCode);

        ShadowDexFile.setIsDexOptNeeded(true);
        DexFixer.fixDexIfNecessary(mMockRuntime);
        assertThat(getReason()).isEqualTo(DexFixerReason.O_MR1_CORRUPTED);
        verifyDexOpt();
    }

    @Test
    public void testFixDexIfNecessary_notReadableWithSplits() {
        ApplicationInfo appInfo = ContextUtils.getApplicationContext().getApplicationInfo();
        appInfo.splitNames = new String[] {"ignored.en"};
        DexFixer.setHasIsolatedSplits(true);
        ShadowOs.sWorldReadable = false;
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.ISOLATED_SPLITS_DEX_COMPILE_VERSION,
                BuildInfo.getInstance().versionCode);

        DexFixer.fixDexIfNecessary(mMockRuntime);
        assertThat(getReason()).isEqualTo(DexFixerReason.NOT_READABLE);
        verifyDexOpt();
    }
}
