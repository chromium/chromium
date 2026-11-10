// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.NotificationChannel;

import androidx.test.filters.SmallTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.settings.SettingsTestRule;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.google_apis.gaia.GaiaId;

import java.util.List;

/**
 * Tests for the price notification setting UI. These tests are not batched because each requires
 * restarting the settings activity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Layout and behavior are dependent on setup params for the activity.")
public class PriceNotificationSettingsFragmentTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final SettingsTestRule<PriceNotificationSettingsFragment> mTestRule =
            new SettingsTestRule<>(PriceNotificationSettingsFragment.class);

    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Mock private IdentityServicesProvider mIdentityServicesProvider;

    @Mock private IdentityManager mIdentityManager;

    @Mock private SigninManager mSigninManager;

    @Mock private PrefService mPrefs;
    @Mock private BaseNotificationManagerProxy mNotificationManagerProxy;
    private WebPageStation mPage;

    @Before
    public void setUp() {
        // Make sure the browser is set up correctly prior to mocking everything for settings.
        mPage = mActivityTestRule.startOnBlankPage();

        when(mIdentityManager.getPrimaryAccountInfo())
                .thenReturn(
                        new AccountInfo.Builder("user@example.com", new GaiaId("12345")).build());
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);
        when(mIdentityServicesProvider.getSigninManager(any())).thenReturn(mSigninManager);

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
        when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(null);

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

    /**
     * Regression test for the async notification manager path: the notification channel lookup
     * can complete after the fragment has been detached, which used to crash with an
     * IllegalStateException from Fragment#getString().
     */
    @Test
    @SmallTest
    @Feature("PriceTrackingSettings")
    @SuppressWarnings("unchecked") // ArgumentCaptor of a generic Callback.
    public void testMobileNotificationsTextCallbackAfterDetachDoesNotCrash() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mNotificationManagerProxy);

        mTestRule.startSettingsActivity();

        ArgumentCaptor<Callback<NotificationChannel>> captor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mNotificationManagerProxy, atLeastOnce())
                .getNotificationChannel(
                        eq(ChromeChannelDefinitions.ChannelId.PRICE_DROP_DEFAULT),
                        captor.capture());
        List<Callback<NotificationChannel>> pendingCallbacks = captor.getAllValues();

        // Tear down the settings activity so the fragment is detached before the channel lookup
        // "completes".
        ChromeBaseAppCompatActivity activity = mTestRule.getActivity();
        ThreadUtils.runOnUiThreadBlocking(activity::finish);
        ApplicationTestUtils.waitForActivityState(activity, Stage.DESTROYED);

        // Must not throw.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (Callback<NotificationChannel> callback : pendingCallbacks) {
                        callback.onResult(null);
                    }
                });
    }

    private ChromeSwitchPreference getEmailNotificationsSwitch() {
        return (ChromeSwitchPreference)
                mTestRule
                        .getFragment()
                        .findPreference(PriceNotificationSettingsFragment.PREF_EMAIL_NOTIFICATIONS);
    }
}
