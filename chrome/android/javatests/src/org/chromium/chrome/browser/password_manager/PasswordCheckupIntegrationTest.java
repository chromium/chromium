// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.GmsCoreVersionRestriction;
import org.chromium.url.GURL;

/** Integration test for accessing password checkup. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "show-autofill-signatures"})
public class PasswordCheckupIntegrationTest {
    private static final String SAFETY_CHECK_INTERACTIONS_HISTOGRAM =
            "Settings.SafetyCheck.Interactions";
    private static final int SAFETY_CHECK_INTERACTION_STARTED = 0;
    private static final int SAFETY_CHECK_INTERACTION_PASSWORDS_MANAGE = 2;
    // The string below needs to be kept in sync with
    // IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_SUBHEADER_RECENTLY.
    private static final String RECENT_PASSWORD_CHECK_LABEL = "Checked just now";
    private static final String SIGNIN_FORM_URL = "/chrome/test/data/password/simple_password.html";
    public static final GURL EXAMPLE_URL = new GURL("https://www.example.com/");
    private static final String USERNAME_TEXT = "test4";
    private static final String PASSWORD_TEXT = "test4";

    @Rule public SyncTestRule mSyncTestRule = new SyncTestRule();

    @Rule public SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule;

    private FakeCredentialManagerLauncherFactoryImpl mFakeCredentialManagerLauncherFactory;

    private FakePasswordCheckupClientHelperFactoryImpl mFakePasswordCheckupClientHelperFactory;

    private FakePasswordCheckupClientHelper mFakePasswordCheckupClientHelper;

    private PendingIntent mPendingIntentForLocalCheckup;

    private PendingIntent mPendingIntentForAccountCheckup;

    private PasswordStoreBridge mPasswordStoreBridge;

    @Before
    public void setup() throws Exception {
        MockitoAnnotations.initMocks(this);

        mSettingsActivityTestRule = new SettingsActivityTestRule<>(MainSettings.class);

        mFakeCredentialManagerLauncherFactory = new FakeCredentialManagerLauncherFactoryImpl();
        CredentialManagerLauncherFactory.setFactoryForTesting(
                mFakeCredentialManagerLauncherFactory);

        Context context = ApplicationProvider.getApplicationContext();
        mPendingIntentForLocalCheckup = createIntentForTesting(context);
        mPendingIntentForAccountCheckup = createIntentForTesting(context);
        mFakeCredentialManagerLauncherFactory.setIntent(createIntentForTesting(context));
        mFakePasswordCheckupClientHelperFactory = new FakePasswordCheckupClientHelperFactoryImpl();
        PasswordCheckupClientHelperFactory.setFactoryForTesting(
                mFakePasswordCheckupClientHelperFactory);
        mFakePasswordCheckupClientHelper =
                (FakePasswordCheckupClientHelper)
                        mFakePasswordCheckupClientHelperFactory.createHelper();
        mFakePasswordCheckupClientHelper.setIntentForLocalCheckup(mPendingIntentForLocalCheckup);
        mFakePasswordCheckupClientHelper.setIntentForAccountCheckup(
                mPendingIntentForAccountCheckup);

        runOnUiThreadBlocking(
                () -> {
                    mPasswordStoreBridge = new PasswordStoreBridge(mSyncTestRule.getProfile(false));
                });
    }

    @After
    public void tearDown() {
        runOnUiThreadBlocking(
                () -> {
                    mPasswordStoreBridge.clearAllPasswords();
                });
    }

    @Test
    @LargeTest
    @Restriction({
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
        GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_22W30
    })
    @DisableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testPasswordCheckOpensPasswordCheckupForLocalWhenStateCompromised() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                SAFETY_CHECK_INTERACTIONS_HISTOGRAM,
                                SAFETY_CHECK_INTERACTION_STARTED)
                        .expectIntRecord(
                                SAFETY_CHECK_INTERACTIONS_HISTOGRAM,
                                SAFETY_CHECK_INTERACTION_PASSWORDS_MANAGE)
                        .build();
        // TODO - b/342101044: Write a test for the non-syncing user.
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        // Store the test credential.
        PasswordStoreCredential testCredential =
                new PasswordStoreCredential(EXAMPLE_URL, USERNAME_TEXT, PASSWORD_TEXT);
        runOnUiThreadBlocking(
                () -> {
                    mPasswordStoreBridge.insertPasswordCredential(testCredential);
                });
        mFakePasswordCheckupClientHelper.setBreachedCredentialsCount(1);

        openSafetyCheckFromChromeSettings();
        onViewWaiting(withText(R.string.safety_check_button)).perform(click());
        onViewWaiting(withText(RECENT_PASSWORD_CHECK_LABEL));

        onViewWaiting(withText(R.string.safety_check_passwords_local_title)).perform(click());

        // Verify that the histogram is recorded.
        histogramWatcher.assertExpected();
        // TODO - b/340836636: Test that the correct intent is called.
    }

    private void scrollToSetting(Matcher<View> matcher) {
        onView(withId(R.id.recycler_view))
                .perform(RecyclerViewActions.scrollTo(hasDescendant(matcher)));
    }

    private void openSafetyCheckFromChromeSettings() {
        mSettingsActivityTestRule.startSettingsActivity();
        scrollToSetting(withText(R.string.prefs_safety_check));
        onView(withText(R.string.prefs_safety_check)).perform(click());
    }

    private PendingIntent createIntentForTesting(Context context) {
        return PendingIntent.getActivity(
                context, 123, new Intent(context, Activity.class), PendingIntent.FLAG_IMMUTABLE);
    }
}
