// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests of {@link LensUtils}.
 * TODO(https://crbug.com/1054738): Reimplement LensUtilsTest as robolectric tests
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class LensUtilsTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private static final String TEST_MIN_AGSA_VERSION = "11.34";
    private static final String TEST_MIN_AGSA_VERSION_BELOW_DIRECT_INTENT_MIN = "11.33.9";

    @Mock
    Context mContext;
    @Mock
    PackageManager mPackageManager;
    @Mock
    PackageInfo mPackageInfo;

    @Before
    public void setUp() throws NameNotFoundException {
        MockitoAnnotations.initMocks(this);
        doReturn(mContext).when(mContext).getApplicationContext();
        doReturn(mPackageManager).when(mContext).getPackageManager();
        doReturn(mPackageInfo).when(mPackageManager).getPackageInfo(IntentHandler.PACKAGE_GSA, 0);
    }

    /**
     * Test {@link LensUtils#isGoogleLensFeatureEnabled()} method when disable incognito param is
     * unset and user is incognito.
     */
    @Test
    @SmallTest
    public void isGoogleLensFeatureEnabled_incognitoParamUnsetIncognitoUser() {
        Assert.assertFalse("Feature incorrectly enabled when incognito param is not set",
                isGoogleLensFeatureEnabledOnUiThread(true));
    }

    /**
     * Test {@link LensUtils#isGoogleLensFeatureEnabled()} method when incognito users are enabled
     * and user is incognito.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:disableOnIncognito/false"})
    @Test
    @SmallTest
    public void
    isGoogleLensFeatureEnabled_incognitoEnabledIncognitoUser() {
        Assert.assertTrue("Feature incorrectly disabled when incognito param is not set",
                isGoogleLensFeatureEnabledOnUiThread(true));
    }

    /**
     * Test {@link LensUtils#isGoogleLensFeatureEnabled()} method when incognito users are disabled
     * and user is incognito.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:disableOnIncognito/true"})
    @Test
    @SmallTest
    public void
    isGoogleLensFeatureEnabled_incognitoDisabledIncognitoUser() {
        Assert.assertFalse("Feature incorrectly not disabled when incognito param was set",
                isGoogleLensFeatureEnabledOnUiThread(true));
    }

    /**
     * Test {@link LensUtils#isGoogleLensFeatureEnabled()} method when incognito users are disabled
     * and user is not incognito.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:disableOnIncognito/true"})
    @Test
    @SmallTest
    public void
    isGoogleLensFeatureEnabled_incognitoDisabledStandardUser() {
        Assert.assertTrue("Feature incorrectly disabled when user was not incognito",
                isGoogleLensFeatureEnabledOnUiThread(false));
    }

    /**
     * Test {@link LensUtils#shouldLogUkmByFeature(featureName)} method for enable UKM logging for
     * Lens chip integration.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:disableOnIncognito/true/"
                    + "logUkm/true"})
    @Test
    @SmallTest
    public void
    shouldLogUkm_translateChipUkmLoggingEnabled() {
        assertTrue(
                shouldLogUkmOnUiThread(ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS));
    }

    /**
     * Test {@link LensUtils#shouldLogUkmByFeature(featureName)} method for enable UKM logging for
     * Lens chip integration.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:disableOnIncognito/true/"
                    + "logUkm/false"})
    @Test
    @SmallTest
    public void
    shouldLogUkm_translateChipUkmLoggingDisabled() {
        assertFalse(
                shouldLogUkmOnUiThread(ChromeFeatureList.CONTEXT_MENU_TRANSLATE_WITH_GOOGLE_LENS));
    }

    /**
     * Test {@link LensUtils#shouldLogUkmByFeature(featureName)} method for enable UKM logging for
     * Lens chip integration.
     */
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:disableOnIncognito/true/"
                    + "logUkm/true"})
    @Test
    @SmallTest
    public void
    shouldLogUkm_shoppingChipUkmLoggingEnabled() {
        assertTrue(shouldLogUkmOnUiThread(ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP));
    }

    /**
     * Test {@link LensUtils#shouldLogUkmByFeature(featureName)} method for enable UKM logging for
     * Lens chip integration.
     */
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:disableOnIncognito/true/"
                    + "logUkm/false"})
    @Test
    @SmallTest
    public void
    shouldLogUkm_shoppingChipUkmLoggingDisabled() {
        assertFalse(shouldLogUkmOnUiThread(ChromeFeatureList.CONTEXT_MENU_GOOGLE_LENS_CHIP));
    }

    /**
     * Test {@link LensUtils#isGoogleLensFeatureEnabledOnTablet()} method when search with Google
     * Lens is enabled.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled"})
    @Test
    @SmallTest
    public void
    isGoogleLensFeatureEnabled_tabletDisabled() {
        Assert.assertFalse("Feature incorrectly enabled when Lens on tablet was disabled",
                isGoogleLensFeatureEnabledOnTabletOnUiThread());
    }

    /**
     * Test {@link LensUtils#isGoogleLensFeatureEnabledOnTablet()} method when search with Google
     * Lens is enabled.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:enableContextMenuSearchOnTablet/true"})
    @Test
    @SmallTest
    public void
    isGoogleLensFeatureEnabled_tabletEnabled() {
        Assert.assertTrue("Feature incorrectly disabled when Lens on tablet was enabled",
                isGoogleLensFeatureEnabledOnTabletOnUiThread());
    }

    private boolean isGoogleLensFeatureEnabledOnUiThread(boolean isIncognito) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> LensUtils.isGoogleLensFeatureEnabled(isIncognito));
    }

    private boolean shouldLogUkmOnUiThread(String featureName) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> LensUtils.shouldLogUkmByFeature(featureName));
    }

    private boolean isGoogleLensFeatureEnabledOnTabletOnUiThread() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> LensUtils.isGoogleLensFeatureEnabledOnTablet());
    }
}
