// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.embedder_support.util.ShadowUrlUtilities;

/** Tests for {@link InstalledWebappRegistrar}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowUrlUtilities.class})
public class InstalledWebappRegistrarTest {
    private static final int APP_UID = 123;
    private static final String APP_NAME = "Example App";
    private static final String APP_PACKAGE = "com.example.app";
    private static final String MISSING_PACKAGE = "com.missing.app";
    private static final String PAGE_URL = "https://www.example.com/";
    private static final Origin ORIGIN = Origin.create(PAGE_URL);
    private static final Origin OTHER_ORIGIN = Origin.create("https://www.other.com/");

    @Mock private PackageManager mPackageManager;

    private InstalledWebappRegistrar mRegistrar;

    private static String transform(String origin) {
        // Just an arbitrary string transformation so we can check it is applied.
        return origin.toUpperCase();
    }

    @Before
    public void setUp() throws PackageManager.NameNotFoundException {
        MockitoAnnotations.initMocks(this);

        ApplicationInfo appInfo = new ApplicationInfo();
        appInfo.uid = APP_UID;

        // Even though we're not actually calling getApplicationInfo here, the code needs to deal
        // with a checked exception.
        doReturn(appInfo).when(mPackageManager).getApplicationInfo(eq(APP_PACKAGE), anyInt());
        doReturn(APP_NAME).when(mPackageManager).getApplicationLabel(appInfo);

        doThrow(new PackageManager.NameNotFoundException())
                .when(mPackageManager)
                .getApplicationInfo(eq(MISSING_PACKAGE), anyInt());

        Context context =
                new ContextWrapper(ContextUtils.getApplicationContext()) {
                    @Override
                    public PackageManager getPackageManager() {
                        return mPackageManager;
                    }
                };
        ContextUtils.initApplicationContextForTests(context);

        ShadowUrlUtilities.setTestImpl(
                new ShadowUrlUtilities.TestImpl() {
                    @Override
                    public String getDomainAndRegistry(
                            String uri, boolean includePrivateRegistries) {
                        return transform(uri);
                    }
                });

        mRegistrar = new InstalledWebappRegistrar();
    }

    @After
    public void tearDown() {
        ShadowUrlUtilities.reset();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void testRegister() {
        mRegistrar.registerClient(APP_PACKAGE, ORIGIN, PAGE_URL);
        verifyRegistration(ORIGIN);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void testDeduplicate() {
        mRegistrar.registerClient(APP_PACKAGE, ORIGIN, PAGE_URL);
        mRegistrar.registerClient(APP_PACKAGE, ORIGIN, PAGE_URL);
        verifyRegistration(ORIGIN);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void testDifferentOrigins() {
        mRegistrar.registerClient(APP_PACKAGE, ORIGIN, PAGE_URL);
        mRegistrar.registerClient(APP_PACKAGE, OTHER_ORIGIN, PAGE_URL);
        verifyRegistration(ORIGIN);
        verifyRegistration(OTHER_ORIGIN);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void testMisingPackage() {
        var uids = InstalledWebappDataRegister.getUids();
        mRegistrar.registerClient(MISSING_PACKAGE, ORIGIN, PAGE_URL);
        // Implicitly checking we don't throw.
        assertEquals(uids, InstalledWebappDataRegister.getUids());
    }

    private void verifyRegistration(Origin origin) {
        assertTrue(
                InstalledWebappDataRegister.getDomainsForRegisteredUid(APP_UID)
                        .contains(transform(origin.toString())));
    }
}
