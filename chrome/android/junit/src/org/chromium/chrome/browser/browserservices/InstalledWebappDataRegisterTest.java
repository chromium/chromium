// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.embedder_support.util.Origin;

import java.util.Set;

/** Tests for {@link InstalledWebappDataRegister}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InstalledWebappDataRegisterTest {
    private static final int UID = 23;
    private static final String APP_NAME = "Example App";
    private static final String APP_PACKAGE = "com.example.app";
    private static final String DOMAIN = "example.com";
    private static final String OTHER_DOMAIN = "otherexample.com";

    @Test
    @Feature("TrustedWebActivities")
    public void registration() {
        register(DOMAIN);

        Assert.assertTrue(InstalledWebappDataRegister.chromeHoldsDataForPackage(UID));
        Assert.assertEquals(APP_NAME, InstalledWebappDataRegister.getAppNameForRegisteredUid(UID));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void deregistration() {
        register(DOMAIN);
        InstalledWebappDataRegister.removePackage(UID);

        Assert.assertFalse(InstalledWebappDataRegister.chromeHoldsDataForPackage(UID));
        Assert.assertNull(InstalledWebappDataRegister.getAppNameForRegisteredUid(UID));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void getOrigins() {
        register(DOMAIN);
        register(OTHER_DOMAIN);

        Set<String> origins = InstalledWebappDataRegister.getDomainsForRegisteredUid(UID);
        Assert.assertEquals(2, origins.size());
        Assert.assertTrue(origins.contains(DOMAIN));
        Assert.assertTrue(origins.contains(OTHER_DOMAIN));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void clearOrigins() {
        register(DOMAIN);
        register(OTHER_DOMAIN);
        InstalledWebappDataRegister.removePackage(UID);

        Set<String> origins = InstalledWebappDataRegister.getDomainsForRegisteredUid(UID);
        Assert.assertTrue(origins.isEmpty());
    }

    @Test
    @Feature("TrustedWebActivities")
    public void getAppName() {
        register(DOMAIN);
        Assert.assertEquals(APP_NAME, InstalledWebappDataRegister.getAppNameForRegisteredUid(UID));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void getPackageName() {
        register(DOMAIN);
        Assert.assertEquals(
                APP_PACKAGE, InstalledWebappDataRegister.getPackageNameForRegisteredUid(UID));
    }

    private void register(String domain) {
        InstalledWebappDataRegister.registerPackageForOrigin(
                UID, APP_NAME, APP_PACKAGE, domain, Origin.create("https://www." + domain));
    }
}
