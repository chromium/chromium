// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing;

import static org.mockito.Mockito.spy;

import android.content.Context;
import android.content.Intent;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Unit tests for PasswordProtectionBroadcastReceiver. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PasswordProtectionBroadcastReceiverTest {
    private static final String LOGIN_ACTION = "com.android.chrome.LOGIN";
    private Context mContext;
    private PasswordProtectionBroadcastReceiver mReceiver;
    private SharedPreferencesManager mPrefManager;

    @Before
    public void setUp() {
        mContext = spy(RuntimeEnvironment.application);
        ContextUtils.initApplicationContextForTests(mContext);
        mReceiver = new PasswordProtectionBroadcastReceiver();
        mPrefManager = ChromeSharedPreferences.getInstance();
    }

    @Test
    public void testExtrasSaved() throws JSONException {
        Intent intent = new Intent(LOGIN_ACTION);
        intent.putExtra(
                PasswordProtectionBroadcastReceiver.EXTRA_ACCOUNT_IDENTIFIER, "user1@gmail.com");
        intent.putExtra(PasswordProtectionBroadcastReceiver.EXTRA_SALT, "salt");
        intent.putExtra(PasswordProtectionBroadcastReceiver.EXTRA_HASHED_PASSWORD, 1L);
        mReceiver.onReceive(mContext, intent);
        JSONArray entries =
                new JSONArray(
                        mPrefManager.readString(
                                ChromePreferenceKeys.PASSWORD_PROTECTION_ACCOUNTS, null));
        Assert.assertEquals(1, entries.length());
        Assert.assertEquals(
                "user1@gmail.com",
                ((JSONObject) entries.get(0))
                        .getString(PasswordProtectionBroadcastReceiver.EXTRA_ACCOUNT_IDENTIFIER));
        Assert.assertEquals(
                "salt",
                ((JSONObject) entries.get(0))
                        .getString(PasswordProtectionBroadcastReceiver.EXTRA_SALT));
        Assert.assertEquals(
                1L,
                ((JSONObject) entries.get(0))
                        .getLong(PasswordProtectionBroadcastReceiver.EXTRA_HASHED_PASSWORD));
    }

    @Test
    public void testExtrasNotSavedOnBadEmptyAccount() throws JSONException {
        Intent intent = new Intent(LOGIN_ACTION);
        intent.putExtra(PasswordProtectionBroadcastReceiver.EXTRA_ACCOUNT_IDENTIFIER, "");
        intent.putExtra(PasswordProtectionBroadcastReceiver.EXTRA_SALT, "salt");
        intent.putExtra(PasswordProtectionBroadcastReceiver.EXTRA_HASHED_PASSWORD, 1L);
        mReceiver.onReceive(mContext, intent);
        Assert.assertNull(
                mPrefManager.readString(ChromePreferenceKeys.PASSWORD_PROTECTION_ACCOUNTS, null));
    }

    @Test
    public void testExtrasNotSavedOnBadSalt() throws JSONException {
        Intent intent = new Intent(LOGIN_ACTION);
        intent.putExtra(
                PasswordProtectionBroadcastReceiver.EXTRA_ACCOUNT_IDENTIFIER, "user1@gmail.com");
        intent.putExtra(PasswordProtectionBroadcastReceiver.EXTRA_SALT, "");
        intent.putExtra(PasswordProtectionBroadcastReceiver.EXTRA_HASHED_PASSWORD, 1L);
        mReceiver.onReceive(mContext, intent);
        Assert.assertNull(
                mPrefManager.readString(ChromePreferenceKeys.PASSWORD_PROTECTION_ACCOUNTS, null));
    }

    @Test
    public void testExtrasNotSavedOnBadHashedPassword() throws JSONException {
        Intent intent = new Intent(LOGIN_ACTION);
        intent.putExtra(
                PasswordProtectionBroadcastReceiver.EXTRA_ACCOUNT_IDENTIFIER, "user1@gmail.com");
        intent.putExtra(PasswordProtectionBroadcastReceiver.EXTRA_SALT, "salt");
        intent.putExtra(PasswordProtectionBroadcastReceiver.EXTRA_HASHED_PASSWORD, 0L);
        mReceiver.onReceive(mContext, intent);
        Assert.assertNull(
                mPrefManager.readString(ChromePreferenceKeys.PASSWORD_PROTECTION_ACCOUNTS, null));
    }

    @Test
    public void testAddingMultipleAccounts() throws JSONException {
        Intent intent = new Intent(LOGIN_ACTION);
        intent.putExtra(
                PasswordProtectionBroadcastReceiver.EXTRA_ACCOUNT_IDENTIFIER, "user1@gmail.com");
        intent.putExtra(PasswordProtectionBroadcastReceiver.EXTRA_SALT, "salt");
        intent.putExtra(PasswordProtectionBroadcastReceiver.EXTRA_HASHED_PASSWORD, 1L);
        mReceiver.onReceive(mContext, intent);
        intent.putExtra(
                PasswordProtectionBroadcastReceiver.EXTRA_ACCOUNT_IDENTIFIER, "user2@gmail.com");
        intent.putExtra(PasswordProtectionBroadcastReceiver.EXTRA_SALT, "salt");
        intent.putExtra(PasswordProtectionBroadcastReceiver.EXTRA_HASHED_PASSWORD, 2L);
        mReceiver.onReceive(mContext, intent);
        JSONArray entries =
                new JSONArray(
                        mPrefManager.readString(
                                ChromePreferenceKeys.PASSWORD_PROTECTION_ACCOUNTS, null));
        Assert.assertEquals(2, entries.length());
        Assert.assertEquals(
                "user1@gmail.com",
                ((JSONObject) entries.get(0))
                        .getString(PasswordProtectionBroadcastReceiver.EXTRA_ACCOUNT_IDENTIFIER));
        Assert.assertEquals(
                "salt",
                ((JSONObject) entries.get(0))
                        .getString(PasswordProtectionBroadcastReceiver.EXTRA_SALT));
        Assert.assertEquals(
                1L,
                ((JSONObject) entries.get(0))
                        .getLong(PasswordProtectionBroadcastReceiver.EXTRA_HASHED_PASSWORD));
        Assert.assertEquals(
                "user2@gmail.com",
                ((JSONObject) entries.get(1))
                        .getString(PasswordProtectionBroadcastReceiver.EXTRA_ACCOUNT_IDENTIFIER));
        Assert.assertEquals(
                "salt",
                ((JSONObject) entries.get(1))
                        .getString(PasswordProtectionBroadcastReceiver.EXTRA_SALT));
        Assert.assertEquals(
                2L,
                ((JSONObject) entries.get(1))
                        .getLong(PasswordProtectionBroadcastReceiver.EXTRA_HASHED_PASSWORD));
    }

    @Test
    public void testAddingSameAccountMultipleTimes() throws JSONException {
        Intent intent = new Intent(LOGIN_ACTION);
        intent.putExtra(
                PasswordProtectionBroadcastReceiver.EXTRA_ACCOUNT_IDENTIFIER, "user1@gmail.com");
        intent.putExtra(PasswordProtectionBroadcastReceiver.EXTRA_SALT, "salt");
        intent.putExtra(PasswordProtectionBroadcastReceiver.EXTRA_HASHED_PASSWORD, 1L);
        mReceiver.onReceive(mContext, intent);
        intent.putExtra(
                PasswordProtectionBroadcastReceiver.EXTRA_ACCOUNT_IDENTIFIER, "user1@gmail.com");
        intent.putExtra(PasswordProtectionBroadcastReceiver.EXTRA_SALT, "salt2");
        intent.putExtra(PasswordProtectionBroadcastReceiver.EXTRA_HASHED_PASSWORD, 2L);
        mReceiver.onReceive(mContext, intent);
        JSONArray entries =
                new JSONArray(
                        mPrefManager.readString(
                                ChromePreferenceKeys.PASSWORD_PROTECTION_ACCOUNTS, null));
        Assert.assertEquals(1, entries.length());
        Assert.assertEquals(
                "user1@gmail.com",
                ((JSONObject) entries.get(0))
                        .getString(PasswordProtectionBroadcastReceiver.EXTRA_ACCOUNT_IDENTIFIER));
        Assert.assertEquals(
                "salt2",
                ((JSONObject) entries.get(0))
                        .getString(PasswordProtectionBroadcastReceiver.EXTRA_SALT));
        Assert.assertEquals(
                2L,
                ((JSONObject) entries.get(0))
                        .getLong(PasswordProtectionBroadcastReceiver.EXTRA_HASHED_PASSWORD));
    }
}
