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

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.Map;

/**
 * Tests of {@link LensUtils}.
 */
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

    private static void configureFeature(String featureName, String... params) {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.setFeatureFlagsOverride(Map.of(featureName, true));
        for (String param : params) {
            String[] keyValue = param.split("=");
            testValues.addFieldTrialParamOverride(featureName, keyValue[0], keyValue[1]);
        }
        FeatureList.setTestValues(testValues);
    }

    @Test
    public void isGoogleLensFeatureEnabled_incognitoParamUnsetIncognitoUser() {
        configureFeature(ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS);
        Assert.assertFalse("Feature incorrectly enabled when incognito param is not set",
                LensUtils.isGoogleLensFeatureEnabled(true));
    }

    @Test
    public void isGoogleLensFeatureEnabled_incognitoEnabledIncognitoUser() {
        configureFeature(
                ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS, "disableOnIncognito=false");
        Assert.assertTrue("Feature incorrectly disabled when incognito param is not set",
                LensUtils.isGoogleLensFeatureEnabled(true));
    }

    @Test
    public void isGoogleLensFeatureEnabled_incognitoDisabledIncognitoUser() {
        configureFeature(
                ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS, "disableOnIncognito=true");
        Assert.assertFalse("Feature incorrectly not disabled when incognito param was set",
                LensUtils.isGoogleLensFeatureEnabled(true));
    }

    @Test
    public void isGoogleLensFeatureEnabled_incognitoDisabledStandardUser() {
        configureFeature(
                ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS, "disableOnIncognito=true");
        Assert.assertTrue("Feature incorrectly disabled when user was not incognito",
                LensUtils.isGoogleLensFeatureEnabled(false));
    }

    @Test
    public void shouldLogUkm_translateChipUkmLoggingEnabled() {
        configureFeature(ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS,
                "disableOnIncognito=true", "logUkm=true");
        assertTrue(LensUtils.shouldLogUkmByFeature(
                ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS));
    }

    @Test
    public void shouldLogUkm_translateChipUkmLoggingDisabled() {
        configureFeature(ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS,
                "disableOnIncognito=true", "logUkm=false");
        assertFalse(LensUtils.shouldLogUkmByFeature(
                ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS));
    }

    @Test
    public void shouldLogUkm_shoppingChipUkmLoggingEnabled() {
        configureFeature(ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP, "disableOnIncognito=true",
                "logUkm=true");
        assertTrue(
                LensUtils.shouldLogUkmByFeature(ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP));
    }

    @Test
    public void shouldLogUkm_shoppingChipUkmLoggingDisabled() {
        configureFeature(ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP, "disableOnIncognito=true",
                "logUkm=false");
        assertFalse(
                LensUtils.shouldLogUkmByFeature(ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP));
    }

    @Test
    public void isGoogleLensFeatureEnabled_tabletEnabledByDefault() {
        configureFeature(ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS);
        Assert.assertTrue("Feature incorrectly disabled when Lens on tablet was enabled",
                LensUtils.isGoogleLensFeatureEnabledOnTablet());
    }

    @Test
    public void isGoogleLensFeatureEnabled_tabletEnabled() {
        configureFeature(ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS,
                "enableContextMenuSearchOnTablet=true");
        Assert.assertTrue("Feature incorrectly disabled when Lens on tablet was enabled",
                LensUtils.isGoogleLensFeatureEnabledOnTablet());
    }
}
