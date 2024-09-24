// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncherFactory;
import org.chromium.chrome.browser.password_manager.FakeCredentialManagerLauncherFactoryImpl;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.GmsCoreVersionRestriction;

/** Integration test for accessing credential manager. */
@RunWith(ChromeJUnit4ClassRunner.class)
// TODO(crbug.com/344665935): Failing when batched, batch this again.
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "show-autofill-signatures"})
public class CredentialManagerIntegrationTest {
    @Rule public SyncTestRule mSyncTestRule = new SyncTestRule();

    @Rule
    public SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(MainSettings.class);

    private FakeCredentialManagerLauncherFactoryImpl mFakeLauncherFactory =
            new FakeCredentialManagerLauncherFactoryImpl();

    final PayloadCallbackHelper<PendingIntent> mSuccessCallbackHelper =
            new PayloadCallbackHelper<>();
    final PayloadCallbackHelper<Exception> mFailureCallbackHelper = new PayloadCallbackHelper<>();

    @Before
    public void setup() throws Exception {
        MockitoAnnotations.initMocks(this);
        CredentialManagerLauncherFactory.setFactoryForTesting(mFakeLauncherFactory);
        mFakeLauncherFactory.setSuccessCallback(mSuccessCallbackHelper::notifyCalled);
        mFakeLauncherFactory.setFailureCallback(mFailureCallbackHelper::notifyCalled);

        Context context = ApplicationProvider.getApplicationContext();
        // The Intent for testing leads to MainSettings because it needed the Activity.
        // The production code would show the passwords in GMSCore.
        mFakeLauncherFactory.setIntent(
                PendingIntent.getActivity(
                        context,
                        123,
                        new Intent(context, MainSettings.class),
                        PendingIntent.FLAG_IMMUTABLE));

        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
    }

    @Test
    @LargeTest
    @Restriction({
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
        GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_22W30
    })
    public void testUseCredentialManagerFromChromeSettings() {
        mSettingsActivityTestRule.startSettingsActivity();
        scrollToSetting(withText(R.string.password_manager_settings_title));
        onView(withText(R.string.password_manager_settings_title)).perform(click());

        // Verify that success callback was called.
        assertNotNull(mSuccessCallbackHelper.getOnlyPayloadBlocking());
        // Verify that failure callback was not called.
        assertEquals(0, mFailureCallbackHelper.getCallCount());
    }

    @Test
    @LargeTest
    @Restriction({
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
        GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_22W30
    })
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/339278945
    @DisableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testUseCredentialManagerFromSafetyCheckForLocal() {
        mSettingsActivityTestRule.startSettingsActivity();
        scrollToSetting(withText(R.string.prefs_safety_check));
        onView(withText(R.string.prefs_safety_check)).perform(click());
        onViewWaiting(withText(R.string.safety_check_passwords_local_title)).perform(click());

        // Verify that success callback was called.
        assertNotNull(mSuccessCallbackHelper.getOnlyPayloadBlocking());
        // Verify that failure callback was not called.
        assertEquals(0, mFailureCallbackHelper.getCallCount());
    }

    @Test
    @LargeTest
    @Restriction({
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
        GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_22W30
    })
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/339278945
    @DisableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testUseCredentialManagerFromSafetyCheckForAccount() {
        mSettingsActivityTestRule.startSettingsActivity();
        scrollToSetting(withText(R.string.prefs_safety_check));
        onView(withText(R.string.prefs_safety_check)).perform(click());
        String testAccount = mSyncTestRule.getPrimaryAccount(ConsentLevel.SYNC).getEmail();
        String checkForAccountText =
                ApplicationProvider.getApplicationContext()
                        .getString(R.string.safety_check_passwords_account_title)
                        .replace("%1$s", testAccount);
        onViewWaiting(withText(checkForAccountText)).perform(click());

        // Verify that success callback was called.
        assertNotNull(mSuccessCallbackHelper.getOnlyPayloadBlocking());
        // Verify that failure callback was not called.
        assertEquals(0, mFailureCallbackHelper.getCallCount());
    }

    private void scrollToSetting(Matcher<View> matcher) {
        onView(withId(R.id.recycler_view))
                .perform(RecyclerViewActions.scrollTo(hasDescendant(matcher)));
    }
}
