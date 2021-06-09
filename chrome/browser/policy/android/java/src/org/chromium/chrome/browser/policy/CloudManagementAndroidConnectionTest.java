// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for CloudManagementAndroidConnection.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class CloudManagementAndroidConnectionTest {
    private static final String CLIENT_ID = "client-id";
    private static final String SAVED_CLIENT_ID = "saved-client-id";

    /* Simple implementation of {@link CloudManagementAndroidConnection} that overrides {@link
     * generateClientIdInternal} for easier testing. */
    private static class FakeCloudManagementAndroidConnectionDelegate
            implements CloudManagementAndroidConnectionDelegate {
        @Override
        public String generateClientId() {
            return CLIENT_ID;
        }
    }

    @Before
    public void setUp() {
        CloudManagementAndroidConnection.setDelegateForTesting(
                new FakeCloudManagementAndroidConnectionDelegate());
    }

    @Test
    @SmallTest
    public void testGetClientId_Generated() {
        Assert.assertEquals(CloudManagementSharedPreferences.readClientId(), "");

        CloudManagementAndroidConnection connection =
                CloudManagementAndroidConnection.getInstance();
        Assert.assertEquals(connection.getClientId(), CLIENT_ID);
        Assert.assertEquals(CloudManagementSharedPreferences.readClientId(), CLIENT_ID);
    }

    @Test
    @SmallTest
    public void testGetClientId_ReadFromSharedPreferences() {
        CloudManagementSharedPreferences.saveClientId(SAVED_CLIENT_ID);

        CloudManagementAndroidConnection connection =
                CloudManagementAndroidConnection.getInstance();
        Assert.assertEquals(connection.getClientId(), SAVED_CLIENT_ID);
    }
}
