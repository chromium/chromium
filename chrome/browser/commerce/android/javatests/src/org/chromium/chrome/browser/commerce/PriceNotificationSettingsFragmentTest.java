// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.google_apis.gaia.GaiaId;

/**
 * Tests for the price notification setting UI. These tests are not batched because each requires
 * restarting the settings activity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Layout and behavior are dependent on setup params for the activity.")
@DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
public class PriceNotificationSettingsFragmentTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final SettingsActivityTestRule<PriceNotificationSettingsFragment> mTestRule =
            new SettingsActivityTestRule<>(PriceNotificationSettingsFragment.class);

    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Mock private IdentityServicesProvider mIdentityServicesProvider;

    @Mock private IdentityManager mIdentityManager;

    @Mock private PrefService mPrefs;
    private WebPageStation mPage;

    @Before
    public void setUp() {
        // Make sure the browser is set up correctly prior to mocking everything for settings.
        mPage = mActivityTestRule.startOnBlankPage();

        when(mIdentityManager.getPrimaryAccountInfo(anyInt()))
                .thenReturn(
                        CoreAccountInfo.createFromEmailAndGaiaId(
                                "user@example.com", new GaiaId("12345")));
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
    }

    @Test
    @SmallTest
    @Feature("PriceTrackingSettings")
    public void testEmailPreferenceToggleVisibleWithValidAccount() {
        // The test suite setup includes a mock account, no extra work needs to be done before
        // starting.
        mTestRule.startSettingsActivity();

        Assert.assertTrue(getEmailNotificationsSwitch().isVisible());
    }

    @Test
    @SmallTest
    @Feature("PriceTrackingSettings")
    public void testEmailPreferenceToggleInvisibleIfNoAccount() {
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(null);

        mTestRule.startSettingsActivity();

        Assert.assertFalse(getEmailNotificationsSwitch().isVisible());
    }

    @Test
    @SmallTest
    @Feature("PriceTrackingSettings")
    public void testToggleUpdatesEmailPref() {
        mTestRule.startSettingsActivity();
        mTestRule.getFragment().setPrefServiceForTesting(mPrefs);

        Assert.assertFalse(getEmailNotificationsSwitch().isChecked());

        ThreadUtils.runOnUiThreadBlocking(() -> getEmailNotificationsSwitch().performClick());
        verify(mPrefs).setBoolean(Pref.PRICE_EMAIL_NOTIFICATIONS_ENABLED, true);

        ThreadUtils.runOnUiThreadBlocking(() -> getEmailNotificationsSwitch().performClick());
        verify(mPrefs).setBoolean(Pref.PRICE_EMAIL_NOTIFICATIONS_ENABLED, false);
    }

    private ChromeSwitchPreference getEmailNotificationsSwitch() {
        return (ChromeSwitchPreference)
                mTestRule
                        .getFragment()
                        .findPreference(PriceNotificationSettingsFragment.PREF_EMAIL_NOTIFICATIONS);
    }
}
