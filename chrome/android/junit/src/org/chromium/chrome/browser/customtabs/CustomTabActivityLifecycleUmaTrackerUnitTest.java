// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.SystemClock;
import android.text.TextUtils;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.CustomTabActivityLifecycleUmaTracker.ClientIdentifierType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.concurrent.TimeUnit;

/** Unit test for {@link CustomTabActivityLifecycleUmaTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(shadows = {ShadowSystemClock.class})
public class CustomTabActivityLifecycleUmaTrackerUnitTest {
    private static final String PACKAGE_A = "com.example.test.package";
    private static final String PACKAGE_B = "org.test.mypackage";
    private static final String REFERRER_A = "android-app://" + PACKAGE_A;
    private static final String REFERRER_B = "android-app://" + PACKAGE_B;
    private static final int TASK_ID_123 = 123;

    private SharedPreferencesManager mPref;

    @Before
    public void setUp() {
        mPref = ChromeSharedPreferences.getInstance();
        mPref.writeLong(
                ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TIMESTAMP, SystemClock.uptimeMillis());
        mPref.writeBoolean(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION, true);

        ShadowSystemClock.advanceBy(1, TimeUnit.MINUTES);
    }

    @After
    public void tearDown() {
        mPref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION);
        mPref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLIENT_PACKAGE);
        mPref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TIMESTAMP);
        mPref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_REFERRER);
        mPref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_TASK_ID);
        mPref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_URL);

        ShadowSystemClock.reset();
    }

    @Test
    public void testRecord_SamePackageName() {
        recordPrefForTesting(PACKAGE_A, null, TASK_ID_123);
        CustomTabActivityLifecycleUmaTracker.recordForRetainableSessions(
                PACKAGE_A, REFERRER_A, TASK_ID_123, mPref, /* launchWithSameUri= */ true);
        assertInteractionRecorded(ClientIdentifierType.PACKAGE_NAME);
    }

    @Test
    public void testRecord_DiffPackageName() {
        recordPrefForTesting(PACKAGE_A, null, TASK_ID_123);
        CustomTabActivityLifecycleUmaTracker.recordForRetainableSessions(
                PACKAGE_B, REFERRER_B, TASK_ID_123, mPref, /* launchWithSameUri= */ true);
        assertInteractionRecorded(ClientIdentifierType.DIFFERENT);
    }

    @Test
    public void testRecord_SameReferrer() {
        recordPrefForTesting(null, REFERRER_A, TASK_ID_123);
        CustomTabActivityLifecycleUmaTracker.recordForRetainableSessions(
                null, REFERRER_A, TASK_ID_123, mPref, /* launchWithSameUri= */ true);
        assertInteractionRecorded(ClientIdentifierType.REFERRER);
    }

    @Test
    public void testRecord_DiffReferrer() {
        recordPrefForTesting(null, REFERRER_A, TASK_ID_123);
        CustomTabActivityLifecycleUmaTracker.recordForRetainableSessions(
                null, REFERRER_B, TASK_ID_123, mPref, /* launchWithSameUri= */ true);
        assertInteractionRecorded(ClientIdentifierType.DIFFERENT);
    }

    @Test
    public void testRecord_Mixed_ReferrerThenPackage() {
        recordPrefForTesting(null, REFERRER_A, TASK_ID_123);
        CustomTabActivityLifecycleUmaTracker.recordForRetainableSessions(
                PACKAGE_A, "Random referral", TASK_ID_123, mPref, /* launchWithSameUri= */ true);
        assertInteractionRecorded(ClientIdentifierType.MIXED);
    }

    @Test
    public void testRecord_Mixed_PackageThenReferrer() {
        recordPrefForTesting(PACKAGE_A, null, TASK_ID_123);
        CustomTabActivityLifecycleUmaTracker.recordForRetainableSessions(
                null, REFERRER_A, TASK_ID_123, mPref, /* launchWithSameUri= */ true);
        assertInteractionRecorded(ClientIdentifierType.MIXED);
    }

    @Test
    public void testRecord_DiffUri() {
        recordPrefForTesting(null, REFERRER_A, TASK_ID_123);
        CustomTabActivityLifecycleUmaTracker.recordForRetainableSessions(
                PACKAGE_A, REFERRER_A, TASK_ID_123, mPref, /* launchWithSameUri= */ false);
        assertNoInteractionRecorded();
    }

    @Test
    public void testRecord_DiffTaskId() {
        recordPrefForTesting(PACKAGE_A, null, TASK_ID_123);
        CustomTabActivityLifecycleUmaTracker.recordForRetainableSessions(
                PACKAGE_A, REFERRER_A, 99, mPref, /* launchWithSameUri= */ true);
        assertInteractionRecorded(ClientIdentifierType.PACKAGE_NAME);
    }

    @Test
    public void testRecord_NoInteraction() {
        mPref.writeBoolean(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION, false);

        recordPrefForTesting(PACKAGE_A, null, TASK_ID_123);
        CustomTabActivityLifecycleUmaTracker.recordForRetainableSessions(
                PACKAGE_A, REFERRER_A, TASK_ID_123, mPref, /* launchWithSameUri= */ true);
        assertNoInteractionRecorded();
    }

    @Test
    public void testGetReferrer() {
        String extraActivityReferrer = "android-app://extra.activity.referrer";
        Uri activityReferrer = Uri.parse("android-app://activity.referrer");
        String extraReferrerName = "android-app://extra.referrer.name";

        Assert.assertEquals(
                "IntentHandler.EXTRA_ACTIVITY_REFERRER should be used.",
                extraActivityReferrer,
                getReferrer(
                        buildMockActivity(
                                extraActivityReferrer, activityReferrer, extraReferrerName)));
        Assert.assertEquals(
                "Activity#getReferrer should be used.",
                activityReferrer.toString(),
                getReferrer(buildMockActivity(null, activityReferrer, extraReferrerName)));
        Assert.assertEquals(
                "Intent.EXTRA_REFERRER should be used.",
                extraReferrerName,
                getReferrer(buildMockActivity(null, null, extraReferrerName)));
    }

    @Test
    public void testGetReferrer_InvalidInputs() {
        Assert.assertTrue(TextUtils.isEmpty(getReferrer(null)));
        Assert.assertTrue(TextUtils.isEmpty(getReferrer(mock(Activity.class))));

        Activity activityWithNoReferral = buildMockActivity(null, null, null);
        Assert.assertTrue(TextUtils.isEmpty(getReferrer(activityWithNoReferral)));
    }

    private void recordPrefForTesting(String prefPackageName, String prefReferrer, int prefTaskId) {
        mPref.writeString(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLIENT_PACKAGE, prefPackageName);
        mPref.writeString(ChromePreferenceKeys.CUSTOM_TABS_LAST_REFERRER, prefReferrer);
        mPref.writeInt(ChromePreferenceKeys.CUSTOM_TABS_LAST_TASK_ID, prefTaskId);
    }

    private void assertInteractionRecorded(@ClientIdentifierType String expectedSuffix) {
        String prefix = "CustomTabs.RetainableSessionsV2.TimeBetweenLaunch";
        String[] suffixes = {
            ClientIdentifierType.REFERRER,
            ClientIdentifierType.PACKAGE_NAME,
            ClientIdentifierType.MIXED,
            ClientIdentifierType.DIFFERENT
        };

        for (String suffix : suffixes) {
            String histogram = prefix + suffix;
            Assert.assertEquals(
                    "<" + histogram + "> record is different.",
                    expectedSuffix.equals(suffix) ? 1 : 0,
                    RecordHistogram.getHistogramTotalCountForTesting(histogram));
        }
    }

    private void assertNoInteractionRecorded() {
        assertInteractionRecorded("");
    }

    // For test readability
    private String getReferrer(Activity activity) {
        return CustomTabActivityLifecycleUmaTracker.getReferrerUriString(activity);
    }

    private Activity buildMockActivity(
            String extraActivityReferrer, Uri activityReferrer, String extraReferrerName) {
        Activity activity = mock(Activity.class);
        Intent intent = mock(Intent.class);

        doReturn(intent).when(activity).getIntent();
        doReturn(activityReferrer).when(activity).getReferrer();
        doReturn(extraActivityReferrer)
                .when(intent)
                .getStringExtra(IntentHandler.EXTRA_ACTIVITY_REFERRER);
        doReturn(extraReferrerName).when(intent).getStringExtra(Intent.EXTRA_REFERRER_NAME);

        return activity;
    }

    @Test
    public void testClientId_SamePackageName() {
        Assert.assertEquals(
                "ClientIdentifierType mismatch.",
                CustomTabActivityLifecycleUmaTracker.ClientIdentifierType.PACKAGE_NAME,
                CustomTabActivityLifecycleUmaTracker.getClientIdentifierType(
                        PACKAGE_A, PACKAGE_A, REFERRER_A, null, TASK_ID_123, 99));
    }

    @Test
    public void testClientId_DiffPackageName() {
        Assert.assertEquals(
                "ClientIdentifierType mismatch.",
                CustomTabActivityLifecycleUmaTracker.ClientIdentifierType.DIFFERENT,
                CustomTabActivityLifecycleUmaTracker.getClientIdentifierType(
                        PACKAGE_A, PACKAGE_B, REFERRER_B, null, TASK_ID_123, TASK_ID_123));
    }

    @Test
    public void testClientId_SameReferrer() {
        Assert.assertEquals(
                "ClientIdentifierType mismatch.",
                CustomTabActivityLifecycleUmaTracker.ClientIdentifierType.REFERRER,
                CustomTabActivityLifecycleUmaTracker.getClientIdentifierType(
                        null, null, REFERRER_A, REFERRER_A, TASK_ID_123, TASK_ID_123));
    }

    @Test
    public void testClientId_DiffReferrer() {
        Assert.assertEquals(
                "ClientIdentifierType mismatch.",
                CustomTabActivityLifecycleUmaTracker.ClientIdentifierType.DIFFERENT,
                CustomTabActivityLifecycleUmaTracker.getClientIdentifierType(
                        null, null, REFERRER_A, REFERRER_B, TASK_ID_123, TASK_ID_123));
    }

    @Test
    public void testClientId_Mixed_ReferrerThenPackage() {
        Assert.assertEquals(
                "ClientIdentifierType mismatch.",
                CustomTabActivityLifecycleUmaTracker.ClientIdentifierType.MIXED,
                CustomTabActivityLifecycleUmaTracker.getClientIdentifierType(
                        PACKAGE_A, null, "Random referral", REFERRER_A, TASK_ID_123, TASK_ID_123));
    }

    @Test
    public void testClientId_Mixed_PackageThenReferrer() {
        Assert.assertEquals(
                "ClientIdentifierType mismatch.",
                CustomTabActivityLifecycleUmaTracker.ClientIdentifierType.MIXED,
                CustomTabActivityLifecycleUmaTracker.getClientIdentifierType(
                        null, PACKAGE_A, REFERRER_A, null, TASK_ID_123, TASK_ID_123));
    }

    @Test
    public void testClientId_DiffTaskId() {
        Assert.assertEquals(
                "ClientIdentifierType mismatch.",
                CustomTabActivityLifecycleUmaTracker.ClientIdentifierType.PACKAGE_NAME,
                CustomTabActivityLifecycleUmaTracker.getClientIdentifierType(
                        PACKAGE_A, PACKAGE_A, REFERRER_A, null, 99, TASK_ID_123));
    }
}
