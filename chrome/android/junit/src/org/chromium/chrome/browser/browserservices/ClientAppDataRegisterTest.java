// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.Set;

/**
 * Tests for {@link ClientAppDataRegister}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ClientAppDataRegisterTest {
    private static final int UID = 23;
    private static final String APP_NAME = "Example App";
    private static final String APP_PACKAGE = "com.example.app";
    private static final String DOMAIN = "example.com";
    private static final String OTHER_DOMAIN = "otherexample.com";

    private ClientAppDataRegister mRegister;

    @Before
    public void setUp() {
        mRegister = new ClientAppDataRegister();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void registration() {
        register(DOMAIN);

        Assert.assertTrue(mRegister.chromeHoldsDataForPackage(UID));
        Assert.assertEquals(APP_NAME, mRegister.getAppNameForRegisteredUid(UID));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void deregistration() {
        register(DOMAIN);
        mRegister.removePackage(UID);

        Assert.assertFalse(mRegister.chromeHoldsDataForPackage(UID));
        Assert.assertNull(mRegister.getAppNameForRegisteredUid(UID));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void getOrigins() {
        register(DOMAIN);
        register(OTHER_DOMAIN);

        Set<String> origins = mRegister.getDomainsForRegisteredUid(UID);
        Assert.assertEquals(2, origins.size());
        Assert.assertTrue(origins.contains(DOMAIN));
        Assert.assertTrue(origins.contains(OTHER_DOMAIN));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void clearOrigins() {
        register(DOMAIN);
        register(OTHER_DOMAIN);
        mRegister.removePackage(UID);

        Set<String> origins = mRegister.getDomainsForRegisteredUid(UID);
        Assert.assertTrue(origins.isEmpty());
    }

    @Test
    @Feature("TrustedWebActivities")
    public void getAppName() {
        register(DOMAIN);
        Assert.assertEquals(APP_NAME, mRegister.getAppNameForRegisteredUid(UID));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void getPackageName() {
        register(DOMAIN);
        Assert.assertEquals(APP_PACKAGE, mRegister.getPackageNameForRegisteredUid(UID));
    }

    private void register(String domain) {
        mRegister.registerPackageForOrigin(UID, APP_NAME, APP_PACKAGE, domain,
                Origin.create("https://www." + domain));
    }
}
