// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gsa;

import static org.mockito.Mockito.doReturn;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.IntentHandler;

/**
 * Tests of {@link GSAState}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class GSAStateUnitTest {
    @Mock
    Context mContext;
    @Mock
    ComponentName mComponentName;
    @Mock
    PackageManager mPackageManager;

    GSAState mGsaState;

    @Before
    public void setUp() throws NameNotFoundException {
        MockitoAnnotations.initMocks(this);

        PackageInfo packageInfo = Mockito.mock(PackageInfo.class);
        doReturn(mContext).when(mContext).getApplicationContext();
        doReturn(mPackageManager).when(mContext).getPackageManager();
        doReturn(IntentHandler.PACKAGE_GSA).when(mComponentName).getPackageName();
        doReturn(packageInfo).when(mPackageManager).getPackageInfo(IntentHandler.PACKAGE_GSA, 0);

        mGsaState = new GSAState(mContext);
    }

    @Test
    public void isAgsaVersionBelowMinimum() {
        Assert.assertFalse(mGsaState.isAgsaVersionBelowMinimum("8.19", "8.19"));
        Assert.assertFalse(mGsaState.isAgsaVersionBelowMinimum("8.19.1", "8.19"));
        Assert.assertFalse(mGsaState.isAgsaVersionBelowMinimum("8.24", "8.19"));
        Assert.assertFalse(mGsaState.isAgsaVersionBelowMinimum("8.25", "8.19"));
        Assert.assertFalse(mGsaState.isAgsaVersionBelowMinimum("8.30", "8.19"));
        Assert.assertFalse(mGsaState.isAgsaVersionBelowMinimum("9.30", "8.19"));

        Assert.assertTrue(mGsaState.isAgsaVersionBelowMinimum("", "8.19"));
        Assert.assertTrue(mGsaState.isAgsaVersionBelowMinimum("8.1", "8.19"));
        Assert.assertTrue(mGsaState.isAgsaVersionBelowMinimum("7.30", "8.19"));
        Assert.assertTrue(mGsaState.isAgsaVersionBelowMinimum("8", "8.19"));
    }

    @Test
    public void canAgsaHandleIntent() {
        Intent intent = Mockito.mock(Intent.class);
        doReturn(IntentHandler.PACKAGE_GSA).when(intent).getPackage();
        doReturn(mComponentName).when(intent).resolveActivity(mPackageManager);

        Assert.assertTrue(mGsaState.canAgsaHandleIntent(intent));
    }

    @Test
    public void canAgsaHandleIntent_PackageDoesNotMatch() {
        Intent intent = Mockito.mock(Intent.class);
        doReturn("com.test.app").when(intent).getPackage();
        doReturn(mComponentName).when(intent).resolveActivity(mPackageManager);

        Assert.assertFalse(mGsaState.canAgsaHandleIntent(intent));
    }

    @Test
    public void canAgsaHandleIntent_ActivityNull() {
        Intent intent = Mockito.mock(Intent.class);
        doReturn(IntentHandler.PACKAGE_GSA).when(intent).getPackage();
        doReturn(null).when(intent).resolveActivity(mPackageManager);

        Assert.assertFalse(mGsaState.canAgsaHandleIntent(intent));
    }

    @Test
    public void canAgsaHandleIntent_PackageInfoNull() throws NameNotFoundException {
        Intent intent = Mockito.mock(Intent.class);
        doReturn(IntentHandler.PACKAGE_GSA).when(intent).getPackage();
        doReturn(mComponentName).when(intent).resolveActivity(mPackageManager);
        doReturn(null).when(mPackageManager).getPackageInfo(IntentHandler.PACKAGE_GSA, 0);

        Assert.assertFalse(mGsaState.canAgsaHandleIntent(intent));
    }

    @Test
    public void parseAgsaMajorMinorVersionAsInteger() {
        String versionName = "11.7";
        Integer value = mGsaState.parseAgsaMajorMinorVersionAsInteger(versionName);
        Assert.assertNotNull(value);
        Assert.assertEquals("Major/minor verisons should be parsed correctly.", 11007, (int) value);

        versionName = "12.72.100.1";
        value = mGsaState.parseAgsaMajorMinorVersionAsInteger(versionName);
        Assert.assertNotNull(value);
        Assert.assertEquals("Verisons after major/minor should be ignored.", 12072, (int) value);

        versionName = "999.999";
        value = mGsaState.parseAgsaMajorMinorVersionAsInteger(versionName);
        Assert.assertNotNull(value);
        Assert.assertEquals("Test the upper edge case.", 999999, (int) value);

        versionName = "0.0";
        value = mGsaState.parseAgsaMajorMinorVersionAsInteger(versionName);
        Assert.assertNotNull(value);
        Assert.assertEquals("Test the lower edge case.", 0, (int) value);

        versionName = "0.1";
        value = mGsaState.parseAgsaMajorMinorVersionAsInteger(versionName);
        Assert.assertNotNull(value);
        Assert.assertEquals("Test 0 for major.", 1, (int) value);

        versionName = "1.0";
        value = mGsaState.parseAgsaMajorMinorVersionAsInteger(versionName);
        Assert.assertNotNull(value);
        Assert.assertEquals("Test 0 for minor.", 1000, (int) value);

        // Exceed maximum for both major/minor.
        versionName = "1000.999";
        value = mGsaState.parseAgsaMajorMinorVersionAsInteger(versionName);
        Assert.assertNull(value);
        versionName = "999.9999";
        value = mGsaState.parseAgsaMajorMinorVersionAsInteger(versionName);
        Assert.assertNull(value);

        // Formatting errors.
        versionName = "999.";
        value = mGsaState.parseAgsaMajorMinorVersionAsInteger(versionName);
        Assert.assertNull(value);
        versionName = ".999";
        value = mGsaState.parseAgsaMajorMinorVersionAsInteger(versionName);
        Assert.assertNull(value);
        versionName = "999";
        value = mGsaState.parseAgsaMajorMinorVersionAsInteger(versionName);
        Assert.assertNull(value);
        versionName = "12.7notavalidversion";
        value = mGsaState.parseAgsaMajorMinorVersionAsInteger(versionName);
        Assert.assertNull(value);
    }
}
