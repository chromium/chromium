// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Unit tests for CloudManagementSharedPreferencesTest. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CloudManagementSharedPreferencesTest {
    private static final String DM_TOKEN = "fake-dm-token";
    private static final String CLIENT_ID = "fake-client-id";

    @Test
    @SmallTest
    public void testSaveDmToken() {
        CloudManagementSharedPreferences.saveDmToken(DM_TOKEN);
        Assert.assertEquals(
                ChromeSharedPreferences.getInstance()
                        .readString(ChromePreferenceKeys.CLOUD_MANAGEMENT_DM_TOKEN, ""),
                DM_TOKEN);
    }

    @Test
    @SmallTest
    public void testDeleteDmToken() {
        CloudManagementSharedPreferences.saveDmToken(DM_TOKEN);
        Assert.assertEquals(
                ChromeSharedPreferences.getInstance()
                        .readString(ChromePreferenceKeys.CLOUD_MANAGEMENT_DM_TOKEN, ""),
                DM_TOKEN);
        CloudManagementSharedPreferences.deleteDmToken();
        Assert.assertEquals(
                ChromeSharedPreferences.getInstance()
                        .readString(ChromePreferenceKeys.CLOUD_MANAGEMENT_DM_TOKEN, ""),
                "");
    }

    @Test
    @SmallTest
    public void testDeleteEmptyDmToken() {
        CloudManagementSharedPreferences.deleteDmToken();
        Assert.assertEquals(
                ChromeSharedPreferences.getInstance()
                        .readString(ChromePreferenceKeys.CLOUD_MANAGEMENT_DM_TOKEN, ""),
                "");
    }

    @Test
    @SmallTest
    public void testReadDmToken() {
        Assert.assertEquals(CloudManagementSharedPreferences.readDmToken(), "");

        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.CLOUD_MANAGEMENT_DM_TOKEN, DM_TOKEN);
        Assert.assertEquals(CloudManagementSharedPreferences.readDmToken(), DM_TOKEN);
    }

    @Test
    @SmallTest
    public void testSaveClientId() {
        CloudManagementSharedPreferences.saveClientId(CLIENT_ID);
        Assert.assertEquals(
                ChromeSharedPreferences.getInstance()
                        .readString(ChromePreferenceKeys.CLOUD_MANAGEMENT_CLIENT_ID, ""),
                CLIENT_ID);
    }

    @Test
    @SmallTest
    public void testReadClientId() {
        Assert.assertEquals(CloudManagementSharedPreferences.readClientId(), "");

        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.CLOUD_MANAGEMENT_CLIENT_ID, CLIENT_ID);
        Assert.assertEquals(CloudManagementSharedPreferences.readClientId(), CLIENT_ID);
    }
}
