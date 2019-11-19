// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.HttpAuthDatabase;
import org.chromium.base.test.util.Feature;

/**
 * Test suite for HttpAuthDatabase.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS)
public class HttpAuthDatabaseTest {
    private static final String TEST_DATABASE = "http_auth_for_HttpAuthDatabaseTest.db";

    @Before
    public void setUp() {
        InstrumentationRegistry.getTargetContext().deleteDatabase(TEST_DATABASE);
    }

    @After
    public void tearDown() {
        InstrumentationRegistry.getTargetContext().deleteDatabase(TEST_DATABASE);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAccessHttpAuthUsernamePassword() {
        HttpAuthDatabase instance = HttpAuthDatabase.newInstance(
                InstrumentationRegistry.getTargetContext(), TEST_DATABASE);

        String host = "http://localhost:8080";
        String realm = "testrealm";
        String userName = "user";
        String password = "password";

        String[] result = instance.getHttpAuthUsernamePassword(host, realm);
        Assert.assertNull(result);

        instance.setHttpAuthUsernamePassword(host, realm, userName, password);
        result = instance.getHttpAuthUsernamePassword(host, realm);
        Assert.assertNotNull(result);
        Assert.assertEquals(userName, result[0]);
        Assert.assertEquals(password, result[1]);

        String newPassword = "newpassword";
        instance.setHttpAuthUsernamePassword(host, realm, userName, newPassword);
        result = instance.getHttpAuthUsernamePassword(host, realm);
        Assert.assertNotNull(result);
        Assert.assertEquals(userName, result[0]);
        Assert.assertEquals(newPassword, result[1]);

        String newUserName = "newuser";
        instance.setHttpAuthUsernamePassword(host, realm, newUserName, newPassword);
        result = instance.getHttpAuthUsernamePassword(host, realm);
        Assert.assertNotNull(result);
        Assert.assertEquals(newUserName, result[0]);
        Assert.assertEquals(newPassword, result[1]);

        instance.setHttpAuthUsernamePassword(host, realm, null, password);
        result = instance.getHttpAuthUsernamePassword(host, realm);
        Assert.assertNotNull(result);
        Assert.assertNull(result[0]);
        Assert.assertEquals(password, result[1]);

        instance.setHttpAuthUsernamePassword(host, realm, userName, null);
        result = instance.getHttpAuthUsernamePassword(host, realm);
        Assert.assertNotNull(result);
        Assert.assertEquals(userName, result[0]);
        Assert.assertEquals(null, result[1]);

        instance.setHttpAuthUsernamePassword(host, realm, null, null);
        result = instance.getHttpAuthUsernamePassword(host, realm);
        Assert.assertNotNull(result);
        Assert.assertNull(result[0]);
        Assert.assertNull(result[1]);

        instance.setHttpAuthUsernamePassword(host, realm, newUserName, newPassword);
        result = instance.getHttpAuthUsernamePassword(host, realm);
        Assert.assertNotNull(result);
        Assert.assertEquals(newUserName, result[0]);
        Assert.assertEquals(newPassword, result[1]);
    }
}
