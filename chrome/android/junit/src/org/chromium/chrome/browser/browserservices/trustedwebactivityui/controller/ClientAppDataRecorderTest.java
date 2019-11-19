// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.ClientAppDataRegister;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.util.test.ShadowUrlUtilities;

/**
 * Tests for {@link ClientAppDataRecorder}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowUrlUtilities.class})
public class ClientAppDataRecorderTest {
    private static final int APP_UID = 123;
    private static final String APP_NAME = "Example App";
    private static final String APP_PACKAGE = "com.example.app";
    private static final String MISSING_PACKAGE = "com.missing.app";
    private static final Origin ORIGIN = Origin.create("https://www.example.com/");
    private static final Origin OTHER_ORIGIN = Origin.create("https://www.other.com/");

    @Mock private ClientAppDataRegister mRegister;
    @Mock private PackageManager mPackageManager;

    private ClientAppDataRecorder mRecorder;

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

        Context context = mock(Context.class);
        when(context.getPackageManager()).thenReturn(mPackageManager);

        ShadowUrlUtilities.setTestImpl(new ShadowUrlUtilities.TestImpl() {
            @Override
            public String getDomainAndRegistry(String uri, boolean includePrivateRegistries) {
                return transform(uri);
            }
        });

        mRecorder = new ClientAppDataRecorder(context, mRegister);
    }

    @After
    public void tearDown() {
        ShadowUrlUtilities.reset();
    }


    @Test
    @Feature("TrustedWebActivities")
    public void testRegister() {
        mRecorder.register(APP_PACKAGE, ORIGIN);
        verifyRegistration(ORIGIN);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void testDeduplicate() {
        mRecorder.register(APP_PACKAGE, ORIGIN);
        mRecorder.register(APP_PACKAGE, ORIGIN);
        verifyRegistration(ORIGIN);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void testDifferentOrigins() {
        mRecorder.register(APP_PACKAGE, ORIGIN);
        mRecorder.register(APP_PACKAGE, OTHER_ORIGIN);
        verifyRegistration(ORIGIN);
        verifyRegistration(OTHER_ORIGIN);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void testMisingPackage() {
        mRecorder.register(MISSING_PACKAGE, ORIGIN);
        // Implicitly checking we don't throw.
        verify(mRegister, never()).registerPackageForOrigin(anyInt(), anyString(), anyString(),
                any(), any());
    }

    private void verifyRegistration(Origin origin) {
        verify(mRegister).registerPackageForOrigin(APP_UID, APP_NAME, APP_PACKAGE,
                transform(origin.toString()), origin);
    }
}
