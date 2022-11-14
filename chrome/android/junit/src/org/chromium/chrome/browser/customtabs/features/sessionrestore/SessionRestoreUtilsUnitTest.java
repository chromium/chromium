// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.sessionrestore;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SessionRestoreUtilsUnitTest {
    private static final String PACKAGE_A = "com.example.test.package";
    private static final String PACKAGE_B = "org.test.mypackage";
    private static final String REFERRER_A = "android-app://" + PACKAGE_A;
    private static final String REFERRER_B = "android-app://" + PACKAGE_B;
    private static final String URL_A = "https://www.google.com";
    private static final String URL_B = "https://www.espn.com";
    private static final int TASK_ID_123 = 123;

    private SharedPreferencesManager mPref;

    @Before
    public void setUp() {
        mPref = SharedPreferencesManager.getInstance();
        mPref.writeBoolean(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION, true);
    }

    @After
    public void tearDown() {
        mPref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION);
        mPref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLIENT_PACKAGE);
        mPref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_REFERRER);
        mPref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_TASK_ID);
        mPref.removeKey(ChromePreferenceKeys.CUSTOM_TABS_LAST_URL);
    }

    @Test
    public void testCanRestore_True_SamePackageName() {
        recordPrefForTesting(PACKAGE_A, null, URL_A, TASK_ID_123);
        Assert.assertTrue("Session is not restorable.",
                SessionRestoreUtils.canRestoreSession(
                        TASK_ID_123, URL_A, mPref, PACKAGE_A, REFERRER_A));
    }

    @Test
    public void testCanRestore_False_DiffPackageName() {
        recordPrefForTesting(PACKAGE_A, null, URL_B, TASK_ID_123);
        Assert.assertFalse("Session is restorable.",
                SessionRestoreUtils.canRestoreSession(
                        TASK_ID_123, URL_B, mPref, PACKAGE_B, REFERRER_B));
    }

    @Test
    public void testCanRestore_True_SameReferrer() {
        recordPrefForTesting(null, REFERRER_A, URL_B, TASK_ID_123);
        Assert.assertTrue("Session is not restorable.",
                SessionRestoreUtils.canRestoreSession(TASK_ID_123, URL_B, mPref, null, REFERRER_A));
    }

    @Test
    public void testCanRestore_False_DiffReferrer() {
        recordPrefForTesting(null, REFERRER_A, URL_A, TASK_ID_123);
        Assert.assertFalse("Session is restorable.",
                SessionRestoreUtils.canRestoreSession(TASK_ID_123, URL_B, mPref, null, REFERRER_B));
    }

    @Test
    public void testCanRestore_True_ReferrerThenPackage() {
        recordPrefForTesting(null, REFERRER_A, URL_A, TASK_ID_123);
        Assert.assertTrue("Session is not restorable.",
                SessionRestoreUtils.canRestoreSession(
                        TASK_ID_123, URL_A, mPref, PACKAGE_A, "Random referral"));
    }

    @Test
    public void testCanRestore_True_PackageThenReferrer() {
        recordPrefForTesting(PACKAGE_A, null, URL_A, TASK_ID_123);
        Assert.assertTrue("Session is not restorable.",
                SessionRestoreUtils.canRestoreSession(TASK_ID_123, URL_A, mPref, null, REFERRER_A));
    }

    @Test
    public void testCanRestore_False_DiffUri() {
        recordPrefForTesting(null, REFERRER_A, URL_A, TASK_ID_123);
        Assert.assertFalse("Session is restorable.",
                SessionRestoreUtils.canRestoreSession(
                        TASK_ID_123, URL_B, mPref, PACKAGE_A, REFERRER_A));
    }

    @Test
    public void testCanRestore_True_DiffTaskId() {
        recordPrefForTesting(PACKAGE_A, null, URL_A, TASK_ID_123);
        Assert.assertTrue("Session is not restorable.",
                SessionRestoreUtils.canRestoreSession(99, URL_A, mPref, PACKAGE_A, REFERRER_A));
    }

    @Test
    public void testCanRestore_False_NoInteraction() {
        mPref.writeBoolean(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLOSE_TAB_INTERACTION, false);

        recordPrefForTesting(PACKAGE_A, null, URL_A, TASK_ID_123);
        Assert.assertFalse("Session is restorable.",
                SessionRestoreUtils.canRestoreSession(
                        TASK_ID_123, URL_A, mPref, PACKAGE_A, REFERRER_A));
    }

    @Test
    public void testClientId_SamePackageName() {
        Assert.assertEquals("ClientIdentifierType mismatch.",
                SessionRestoreUtils.ClientIdentifierType.PACKAGE_NAME,
                SessionRestoreUtils.getClientIdentifierType(
                        PACKAGE_A, PACKAGE_A, REFERRER_A, null, TASK_ID_123, 99));
    }

    @Test
    public void testClientId_DiffPackageName() {
        Assert.assertEquals("ClientIdentifierType mismatch.",
                SessionRestoreUtils.ClientIdentifierType.DIFFERENT,
                SessionRestoreUtils.getClientIdentifierType(
                        PACKAGE_A, PACKAGE_B, REFERRER_B, null, TASK_ID_123, TASK_ID_123));
    }

    @Test
    public void testClientId_SameReferrer() {
        Assert.assertEquals("ClientIdentifierType mismatch.",
                SessionRestoreUtils.ClientIdentifierType.REFERRER,
                SessionRestoreUtils.getClientIdentifierType(
                        null, null, REFERRER_A, REFERRER_A, TASK_ID_123, TASK_ID_123));
    }

    @Test
    public void testClientId_DiffReferrer() {
        Assert.assertEquals("ClientIdentifierType mismatch.",
                SessionRestoreUtils.ClientIdentifierType.DIFFERENT,
                SessionRestoreUtils.getClientIdentifierType(
                        null, null, REFERRER_A, REFERRER_B, TASK_ID_123, TASK_ID_123));
    }

    @Test
    public void testClientId_Mixed_ReferrerThenPackage() {
        Assert.assertEquals("ClientIdentifierType mismatch.",
                SessionRestoreUtils.ClientIdentifierType.MIXED,
                SessionRestoreUtils.getClientIdentifierType(
                        PACKAGE_A, null, "Random referral", REFERRER_A, TASK_ID_123, TASK_ID_123));
    }

    @Test
    public void testClientId_Mixed_PackageThenReferrer() {
        Assert.assertEquals("ClientIdentifierType mismatch.",
                SessionRestoreUtils.ClientIdentifierType.MIXED,
                SessionRestoreUtils.getClientIdentifierType(
                        null, PACKAGE_A, REFERRER_A, null, TASK_ID_123, TASK_ID_123));
    }

    @Test
    public void testClientId_DiffTaskId() {
        Assert.assertEquals("ClientIdentifierType mismatch.",
                SessionRestoreUtils.ClientIdentifierType.PACKAGE_NAME,
                SessionRestoreUtils.getClientIdentifierType(
                        PACKAGE_A, PACKAGE_A, REFERRER_A, null, 99, TASK_ID_123));
    }

    private void recordPrefForTesting(
            String prefPackageName, String prefReferrer, String prefUrl, int prefTaskId) {
        mPref.writeString(ChromePreferenceKeys.CUSTOM_TABS_LAST_CLIENT_PACKAGE, prefPackageName);
        mPref.writeString(ChromePreferenceKeys.CUSTOM_TABS_LAST_REFERRER, prefReferrer);
        mPref.writeString(ChromePreferenceKeys.CUSTOM_TABS_LAST_URL, prefUrl);
        mPref.writeInt(ChromePreferenceKeys.CUSTOM_TABS_LAST_TASK_ID, prefTaskId);
    }
}
