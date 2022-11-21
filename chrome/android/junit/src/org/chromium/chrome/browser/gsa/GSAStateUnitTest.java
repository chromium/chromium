// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gsa;

import android.content.ComponentName;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.NameNotFoundException;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests of {@link GSAState}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class GSAStateUnitTest {
    private GSAState mGsaState;

    @Before
    public void setUp() throws NameNotFoundException {
        mGsaState = new GSAState();
    }

    private static void initPackageManager(boolean addResolveInfo) {
        ShadowPackageManager pm =
                Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager());
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = GSAState.PACKAGE_NAME;
        pm.installPackage(packageInfo);

        if (addResolveInfo) {
            ComponentName componentName = new ComponentName(GSAState.PACKAGE_NAME, "Activity");
            pm.addActivityIfNotPresent(componentName);
            IntentFilter intentFilter = new IntentFilter(Intent.ACTION_SEARCH);
            intentFilter.addCategory(Intent.CATEGORY_DEFAULT);
            try {
                pm.addIntentFilterForActivity(componentName, intentFilter);
            } catch (NameNotFoundException e) {
                throw new RuntimeException(e);
            }
        }
    }

    private static Intent createAgsaIntent() {
        Intent intent = new Intent(Intent.ACTION_SEARCH);
        intent.setPackage(GSAState.PACKAGE_NAME);
        return intent;
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
        initPackageManager(true);
        Intent intent = createAgsaIntent();
        Assert.assertTrue(mGsaState.canAgsaHandleIntent(intent));
    }

    @Test
    public void canAgsaHandleIntent_PackageDoesNotMatch() {
        initPackageManager(true);
        Intent intent = createAgsaIntent();
        intent.setPackage("com.test.app");
        Assert.assertFalse(mGsaState.canAgsaHandleIntent(intent));
    }

    @Test
    public void canAgsaHandleIntent_ActivityNull() {
        initPackageManager(false);
        Intent intent = createAgsaIntent();
        Assert.assertFalse(mGsaState.canAgsaHandleIntent(intent));
    }

    @Test
    public void canAgsaHandleIntent_PackageInfoNull() throws NameNotFoundException {
        Intent intent = createAgsaIntent();
        // Setting intent ComponentInfo causes intent.resolveActivity() to short-circuit.
        intent.setClassName(GSAState.PACKAGE_NAME, "class");
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
