// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/** Unit tests for {@link EducationalTipModuleUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EducationalTipModuleUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;

    private ObservableSupplierImpl<Profile> mProfileSupplier;

    @Before
    public void setUp() {
        mProfileSupplier = new ObservableSupplierImpl<>();
        mProfileSupplier.set(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        TrackerFactory.setTrackerForTests(mTracker);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance()
                .removeKey(
                        ChromePreferenceKeys
                                .EDUCATIONAL_TIP_DEFAULT_BROWSER_PROMO_ALLOW_DISPLAY_FOR_RELAUNCH);
    }

    @Test
    public void testSetDefaultBrowserPromoAllowDisplayForRelaunchToSharedPreference_True() {
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK))
                .thenReturn(true);

        EducationalTipModuleUtils.setDefaultBrowserPromoAllowDisplayForRelaunchToSharedPreference(
                mProfileSupplier);

        assertTrue(
                EducationalTipModuleUtils
                        .getDefaultBrowserPromoAllowDisplayForRelaunchFromSharedPreference());
    }

    @Test
    public void testSetDefaultBrowserPromoAllowDisplayForRelaunchToSharedPreference_False() {
        when(mTracker.wouldTriggerHelpUi(FeatureConstants.DEFAULT_BROWSER_PROMO_MAGIC_STACK))
                .thenReturn(false);

        EducationalTipModuleUtils.setDefaultBrowserPromoAllowDisplayForRelaunchToSharedPreference(
                mProfileSupplier);

        assertFalse(
                EducationalTipModuleUtils
                        .getDefaultBrowserPromoAllowDisplayForRelaunchFromSharedPreference());
    }

    @Test
    public void testGetDefaultBrowserPromoAllowDisplayForRelaunchFromSharedPreference() {
        // Test default value.
        assertFalse(
                EducationalTipModuleUtils
                        .getDefaultBrowserPromoAllowDisplayForRelaunchFromSharedPreference());

        // Test with true value.
        ChromeSharedPreferences.getInstance()
                .getEditor()
                .putBoolean(
                        ChromePreferenceKeys
                                .EDUCATIONAL_TIP_DEFAULT_BROWSER_PROMO_ALLOW_DISPLAY_FOR_RELAUNCH,
                        true)
                .commit();
        assertTrue(
                EducationalTipModuleUtils
                        .getDefaultBrowserPromoAllowDisplayForRelaunchFromSharedPreference());

        // Test with false value.
        ChromeSharedPreferences.getInstance()
                .getEditor()
                .putBoolean(
                        ChromePreferenceKeys
                                .EDUCATIONAL_TIP_DEFAULT_BROWSER_PROMO_ALLOW_DISPLAY_FOR_RELAUNCH,
                        false)
                .commit();
        assertFalse(
                EducationalTipModuleUtils
                        .getDefaultBrowserPromoAllowDisplayForRelaunchFromSharedPreference());
    }
}
