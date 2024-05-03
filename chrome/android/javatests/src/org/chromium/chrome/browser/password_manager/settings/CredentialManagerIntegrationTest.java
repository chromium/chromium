// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncherFactory;
import org.chromium.chrome.browser.password_manager.FakeCredentialManagerLauncherFactoryImpl;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;

/** Integration test for accessing credential manager. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
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
    @MediumTest
    public void testUseCredentialManagerFromChromeSettings() {
        mSettingsActivityTestRule.startSettingsActivity();
        onView(withText(R.string.password_manager_settings_title)).perform(click());

        // Verify that success callback was called.
        assertNotNull(mSuccessCallbackHelper.getOnlyPayloadBlocking());
        // Verify that failure callback was not called.
        assertEquals(0, mFailureCallbackHelper.getCallCount());
    }
}
