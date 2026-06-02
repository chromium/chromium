// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.content.Context;
import android.content.SharedPreferences;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.embedder_support.util.Origin;

import java.util.HashSet;
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

    @Before
    public void setUp() {
        InstalledWebappDataRegister.setPreferencesForTesting(null);

        ContextUtils.getApplicationContext()
                .getSharedPreferences("trusted_web_activity_client_apps", Context.MODE_PRIVATE)
                .edit()
                .clear()
                .commit();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void registration() {
        register(DOMAIN);

        Assert.assertTrue(InstalledWebappDataRegister.chromeHoldsDataForPackage(APP_PACKAGE));
        Assert.assertEquals(
                APP_NAME, InstalledWebappDataRegister.getAppNameForRegisteredPackage(APP_PACKAGE));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void deregistration() {
        register(DOMAIN);
        InstalledWebappDataRegister.removePackage(APP_PACKAGE);

        Assert.assertFalse(InstalledWebappDataRegister.chromeHoldsDataForPackage(APP_PACKAGE));
        Assert.assertNull(InstalledWebappDataRegister.getAppNameForRegisteredPackage(APP_PACKAGE));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void testMigration() {
        // 1. Populate old data manually
        SharedPreferences prefs =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(
                                "trusted_web_activity_client_apps", Context.MODE_PRIVATE);

        Set<String> uids = new HashSet<>();
        uids.add(String.valueOf(UID));
        prefs.edit().putStringSet("trusted_web_activity_uids", uids).commit();

        prefs.edit().putString(UID + ".appName", APP_NAME).commit();
        prefs.edit().putString(UID + ".packageName", APP_PACKAGE).commit();

        Set<String> domains = new HashSet<>();
        domains.add(DOMAIN);
        prefs.edit().putStringSet(UID + ".domain", domains).commit();

        Set<String> origins = new HashSet<>();
        origins.add("https://www." + DOMAIN);
        prefs.edit().putStringSet(UID + ".origin", origins).commit();

        // 2. Run migration
        InstalledWebappDataRegister.migrateOldDataIfNeeded();

        // 3. Verify new data
        Assert.assertTrue(InstalledWebappDataRegister.chromeHoldsDataForPackage(APP_PACKAGE));
        Assert.assertEquals(
                APP_NAME, InstalledWebappDataRegister.getAppNameForRegisteredPackage(APP_PACKAGE));

        Set<String> migratedDomains =
                InstalledWebappDataRegister.getDomainsForRegisteredPackage(APP_PACKAGE);
        Assert.assertTrue(migratedDomains.contains(DOMAIN));

        // 4. Verify old data is gone
        Assert.assertFalse(prefs.contains("trusted_web_activity_uids"));
        Assert.assertFalse(prefs.contains(UID + ".appName"));
        Assert.assertFalse(prefs.contains(UID + ".packageName"));
        Assert.assertFalse(prefs.contains(UID + ".domain"));
        Assert.assertFalse(prefs.contains(UID + ".origin"));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void getOrigins() {
        register(DOMAIN);
        register(OTHER_DOMAIN);

        Set<String> origins =
                InstalledWebappDataRegister.getDomainsForRegisteredPackage(APP_PACKAGE);
        Assert.assertEquals(2, origins.size());
        Assert.assertTrue(origins.contains(DOMAIN));
        Assert.assertTrue(origins.contains(OTHER_DOMAIN));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void clearOrigins() {
        register(DOMAIN);
        register(OTHER_DOMAIN);
        InstalledWebappDataRegister.removePackage(APP_PACKAGE);

        Set<String> origins =
                InstalledWebappDataRegister.getDomainsForRegisteredPackage(APP_PACKAGE);
        Assert.assertTrue(origins.isEmpty());
    }

    @Test
    @Feature("TrustedWebActivities")
    public void getAppName() {
        register(DOMAIN);
        Assert.assertEquals(
                APP_NAME, InstalledWebappDataRegister.getAppNameForRegisteredPackage(APP_PACKAGE));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void testMultiplePackagesSameUid() {
        String otherPackage = "com.other.app";
        InstalledWebappDataRegister.registerPackageForOrigin(
                "Other App",
                otherPackage,
                OTHER_DOMAIN,
                Origin.create("https://www." + OTHER_DOMAIN));
        register(DOMAIN);

        Assert.assertTrue(InstalledWebappDataRegister.chromeHoldsDataForPackage(APP_PACKAGE));
        Assert.assertTrue(InstalledWebappDataRegister.chromeHoldsDataForPackage(otherPackage));

        Set<String> appDomains =
                InstalledWebappDataRegister.getDomainsForRegisteredPackage(APP_PACKAGE);
        Assert.assertEquals(1, appDomains.size());
        Assert.assertTrue(appDomains.contains(DOMAIN));

        Set<String> otherDomains =
                InstalledWebappDataRegister.getDomainsForRegisteredPackage(otherPackage);
        Assert.assertEquals(1, otherDomains.size());
        Assert.assertTrue(otherDomains.contains(OTHER_DOMAIN));

        InstalledWebappDataRegister.removePackage(APP_PACKAGE);
        Assert.assertFalse(InstalledWebappDataRegister.chromeHoldsDataForPackage(APP_PACKAGE));
        Assert.assertTrue(InstalledWebappDataRegister.chromeHoldsDataForPackage(otherPackage));
    }

    private void register(String domain) {
        InstalledWebappDataRegister.registerPackageForOrigin(
                APP_NAME, APP_PACKAGE, domain, Origin.create("https://www." + domain));
    }
}
