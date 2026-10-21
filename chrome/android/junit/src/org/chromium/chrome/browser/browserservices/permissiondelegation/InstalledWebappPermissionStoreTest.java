// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.browser.trusted.Token;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.embedder_support.util.Origin;

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.HashSet;

/** Unit tests for {@link InstalledWebappPermissionStore}. */
@RunWith(BaseRobolectricTestRunner.class)
public class InstalledWebappPermissionStoreTest {
    private static final Origin ORIGIN_1 = Origin.create("https://www.example.com");
    private static final Origin ORIGIN_2 = Origin.create("https://www.other.com");

    private InstalledWebappPermissionStore mStore;

    @Before
    public void setUp() {
        mStore = new InstalledWebappPermissionStore();
        mStore.clearForTesting();
    }

    @After
    public void tearDown() {
        mStore.clearForTesting();
    }

    @Test
    @Feature({"TrustedWebActivities"})
    public void testGetDelegateAppCount() {
        int[] notificationCount = new int[1];
        mStore.setListener(() -> notificationCount[0]++);

        assertEquals(0, mStore.getDelegateAppCount(ORIGIN_1));
        assertEquals(0, mStore.getDelegateAppCount(ORIGIN_2));

        Token token1 = createToken("com.example.app1");
        Token token2 = createToken("com.example.app2");

        mStore.addDelegateApp(ORIGIN_1, token1);
        assertEquals(1, mStore.getDelegateAppCount(ORIGIN_1));
        assertEquals(0, mStore.getDelegateAppCount(ORIGIN_2));
        assertEquals(1, notificationCount[0]);

        // Adding the same token again does not increase the count or notify the listener.
        mStore.addDelegateApp(ORIGIN_1, token1);
        assertEquals(1, mStore.getDelegateAppCount(ORIGIN_1));
        assertEquals(1, notificationCount[0]);

        // Adding a second distinct token increases the count to 2 and notifies the listener.
        mStore.addDelegateApp(ORIGIN_1, token2);
        assertEquals(2, mStore.getDelegateAppCount(ORIGIN_1));
        assertEquals(2, notificationCount[0]);

        // Removing the origin clears its delegate apps and notifies the listener.
        mStore.removeOrigin(ORIGIN_1);
        assertEquals(0, mStore.getDelegateAppCount(ORIGIN_1));
        assertEquals(3, notificationCount[0]);

        // Removing an origin that is not stored does not notify the listener.
        mStore.removeOrigin(ORIGIN_1);
        assertEquals(3, notificationCount[0]);
    }

    @Test
    @Feature({"TrustedWebActivities"})
    public void testGetDelegateAppCount_doesNotDeserializeTokens() {
        SharedPreferences prefs =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(
                                InstalledWebappPermissionStore.SHARED_PREFS_FILE,
                                Context.MODE_PRIVATE);
        prefs.edit()
                .putStringSet(
                        InstalledWebappPermissionStore.createAllDelegateAppsKey(ORIGIN_1),
                        new HashSet<>(Arrays.asList("not_a_serialized_token_1", "not_a_token_2")))
                .apply();

        assertEquals(2, mStore.getDelegateAppCount(ORIGIN_1));
    }

    private static Token createToken(String packageName) {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        DataOutputStream writer = new DataOutputStream(baos);
        byte[] fingerprint = packageName.getBytes(StandardCharsets.UTF_8);
        try {
            writer.writeUTF(packageName);
            writer.writeInt(1);
            writer.writeInt(fingerprint.length);
            writer.write(fingerprint);
            writer.flush();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        return Token.deserialize(baos.toByteArray());
    }
}
