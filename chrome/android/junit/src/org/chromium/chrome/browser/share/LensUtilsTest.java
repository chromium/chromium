// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.pm.PackageInfo;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Tests of {@link LensUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
public class LensUtilsTest {
    @Before
    public void setUp() {
        ShadowPackageManager pm =
                Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager());
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = IntentHandler.PACKAGE_GSA;
        pm.installPackage(packageInfo);
    }

    @Test
    public void isGoogleLensFeatureEnabled_incognito() {
        Assert.assertFalse(
                "Feature incorrectly enabled when incognito",
                LensUtils.isGoogleLensFeatureEnabled(true));
    }

    @Test
    public void isGoogleLensFeatureEnabled_standard() {
        Assert.assertTrue(
                "Feature incorrectly enabled when non-incognito",
                LensUtils.isGoogleLensFeatureEnabled(false));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS
                    + ":disableOnIncognito/true/logUkm/true")
    public void shouldLogUkm_translateChipUkmLoggingEnabled() {
        assertTrue(
                LensUtils.shouldLogUkmByFeature(
                        ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS));
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS
                    + ":disableOnIncognito/true/logUkm/false")
    public void shouldLogUkm_translateChipUkmLoggingDisabled() {
        assertFalse(
                LensUtils.shouldLogUkmByFeature(
                        ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS));
    }
}
