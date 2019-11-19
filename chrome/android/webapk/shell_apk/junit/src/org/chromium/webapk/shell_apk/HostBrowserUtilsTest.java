// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;

import androidx.annotation.IntDef;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;

/** Tests for HostBrowserUtils. */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(sdk = LocalRobolectricTestRunner.DEFAULT_SDK, manifest = Config.NONE)
public class HostBrowserUtilsTest {
    @IntDef({DefaultBrowserWebApkSupport.YES, DefaultBrowserWebApkSupport.NO})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DefaultBrowserWebApkSupport {
        int YES = 0;
        int NO = 1;
    }

    private static final String DEFAULT_BROWSER_SUPPORTING_WEBAPKS =
            "com.google.android.apps.chrome";
    private static final String DEFAULT_BROWSER_NOT_SUPPORTING_WEBAPKS =
            "com.browser.not.supporting.webapks";
    private static final String[] BROWSERS_SUPPORTING_WEBAPKS =
            new String[] {"com.chrome.canary", "com.chrome.dev", "com.chrome.beta"};
    private static final String[] BROWSERS_NOT_SUPPORTING_WEBAPKS =
            new String[] {"com.random.browser1", "com.random.browser2"};
    private static final String[] ALL_BROWSERS =
            mergeStringArrays(new String[] {DEFAULT_BROWSER_SUPPORTING_WEBAPKS,
                                      DEFAULT_BROWSER_NOT_SUPPORTING_WEBAPKS},
                    BROWSERS_SUPPORTING_WEBAPKS, BROWSERS_NOT_SUPPORTING_WEBAPKS);

    private Context mContext;
    private SharedPreferences mSharedPrefs;
    private TestBrowserInstaller mBrowserInstaller = new TestBrowserInstaller();

    // Whether we are testing with bound WebAPKs.
    private boolean mIsBoundWebApk;

    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {{true}, {false}});
    }

    public HostBrowserUtilsTest(boolean isBoundWebApk) {
        mIsBoundWebApk = isBoundWebApk;
    }

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;

        mSharedPrefs = WebApkSharedPreferences.getPrefs(mContext);

        HostBrowserUtils.resetCachedHostPackageForTesting();
    }

    /**
     * Tests that both {@link #computeHostBrowserPackageClearCachedDataOnChange()}
     * and {@link #getCachedHostBrowserPackage()} return null if there isn't any
     * browser installed on the device.
     */
    @Test
    public void testReturnsNullWhenNoBrowserInstalled() {
        if (mIsBoundWebApk) {
            // Bound browser in AndroidManifest.xml is no longer installed.
            setHostBrowserInMetadata(BROWSERS_SUPPORTING_WEBAPKS[0]);
        }
        Assert.assertNull(
                HostBrowserUtils.computeHostBrowserPackageClearCachedDataOnChange(mContext));
        Assert.assertNull(HostBrowserUtils.getCachedHostBrowserPackage(mContext));
    }

    /**
     * Tests the order of precedence for bound WebAPKs for
     * {@link #computeHostBrowserPackageClearCachedDataOnChange()}. The expected order of precedence
     * is:
     * 1) Browser specified in shared preferences if it is still installed.
     * 2) Bound browser specified in AndroidManifest.xml
     * The default browser and the number of installed browsers which support WebAPKs should
     * have no effect.
     */
    @Test
    public void testComputeHostBrowserBoundWebApkPrecedence() {
        if (!mIsBoundWebApk) return;

        final String boundBrowserSupportingWebApks = BROWSERS_SUPPORTING_WEBAPKS[0];
        setHostBrowserInMetadata(boundBrowserSupportingWebApks);

        // Shared pref browser: Still installed
        // Bound browser in AndroidManifest.xml: Still installed
        {
            final String sharedPrefBrowserSupportingWebApks = BROWSERS_SUPPORTING_WEBAPKS[1];
            setInstalledBrowsersAndClearedCachedData(DefaultBrowserWebApkSupport.YES, ALL_BROWSERS);
            setHostBrowserInSharedPreferences(sharedPrefBrowserSupportingWebApks);
            Assert.assertEquals(sharedPrefBrowserSupportingWebApks,
                    HostBrowserUtils.computeHostBrowserPackageClearCachedDataOnChange(mContext));
        }

        // Shared pref browser: Null
        // Bound browser in AndroidManifest.xml: Still installed
        setInstalledBrowsersAndClearedCachedData(DefaultBrowserWebApkSupport.YES, ALL_BROWSERS);
        setHostBrowserInSharedPreferences(null);
        Assert.assertEquals(boundBrowserSupportingWebApks,
                HostBrowserUtils.computeHostBrowserPackageClearCachedDataOnChange(mContext));

        // Shared pref browser: No longer installed
        // Bound browser in AndroidManifest.xml: Still installed
        setInstalledBrowsersAndClearedCachedData(DefaultBrowserWebApkSupport.YES, ALL_BROWSERS);
        setHostBrowserInSharedPreferences(null);
        Assert.assertEquals(boundBrowserSupportingWebApks,
                HostBrowserUtils.computeHostBrowserPackageClearCachedDataOnChange(mContext));

        // Shared pref browser: Null
        // Bound browser in AndroidManifest.xml: No longer installed
        // Should ignore default browser and number of browsers supporting WebAPKs.
        setInstalledBrowsersAndClearedCachedData(DefaultBrowserWebApkSupport.YES, null);
        setHostBrowserInSharedPreferences(null);
        Assert.assertNull(
                HostBrowserUtils.computeHostBrowserPackageClearCachedDataOnChange(mContext));
    }

    /**
     * Tests the order of precedence for unbound WebAPKs for
     * {@link #computeHostBrowserPackageClearCachedDataOnChange()}. The expected order of precedence
     * is:
     * 1) Browser specified in shared preferences if it is still installed.
     * 2) Default browser if the default browser supports WebAPKs.
     * 3) The browser which supports WebAPKs if there is just one.
     */
    @Test
    public void testComputeHostBrowserUnboundWebApkPrecedence() {
        if (mIsBoundWebApk) return;

        // Shared pref browser: Still installed
        // Default browser: Supports WebAPKs
        {
            final String sharedPrefBrowserSupportingWebApks = BROWSERS_SUPPORTING_WEBAPKS[0];
            setInstalledBrowsersAndClearedCachedData(DefaultBrowserWebApkSupport.YES, ALL_BROWSERS);
            setHostBrowserInSharedPreferences(sharedPrefBrowserSupportingWebApks);
            Assert.assertEquals(sharedPrefBrowserSupportingWebApks,
                    HostBrowserUtils.computeHostBrowserPackageClearCachedDataOnChange(mContext));
        }

        // Shared pref browser: Null
        // Default browser: Supports WebAPKs
        setInstalledBrowsersAndClearedCachedData(DefaultBrowserWebApkSupport.YES, ALL_BROWSERS);
        setHostBrowserInSharedPreferences(null);
        Assert.assertEquals(DEFAULT_BROWSER_SUPPORTING_WEBAPKS,
                HostBrowserUtils.computeHostBrowserPackageClearCachedDataOnChange(mContext));

        // Shared pref browser: Null
        // Default browser: Does not support WebAPKs
        // > 1 installed browsers supporting WebAPKs
        setInstalledBrowsersAndClearedCachedData(DefaultBrowserWebApkSupport.NO, ALL_BROWSERS);
        setHostBrowserInSharedPreferences(null);
        Assert.assertNull(
                HostBrowserUtils.computeHostBrowserPackageClearCachedDataOnChange(mContext));

        // Shared pref browser: Null
        // Default browser: Does not support WebAPKS
        // 1 installed browser supporting WebAPKS
        setInstalledBrowsersAndClearedCachedData(DefaultBrowserWebApkSupport.NO,
                new String[] {BROWSERS_SUPPORTING_WEBAPKS[0], BROWSERS_NOT_SUPPORTING_WEBAPKS[0],
                        BROWSERS_NOT_SUPPORTING_WEBAPKS[1]});
        setHostBrowserInSharedPreferences(null);
        Assert.assertEquals(BROWSERS_SUPPORTING_WEBAPKS[0],
                HostBrowserUtils.computeHostBrowserPackageClearCachedDataOnChange(mContext));

        // Shared pref browser: Null
        // Default browser: Does not support WebAPKs
        // > 1 installed browsers
        setInstalledBrowsersAndClearedCachedData(DefaultBrowserWebApkSupport.NO,
                new String[] {
                        BROWSERS_NOT_SUPPORTING_WEBAPKS[0], BROWSERS_NOT_SUPPORTING_WEBAPKS[1]});
        setHostBrowserInSharedPreferences(null);
        Assert.assertEquals(DEFAULT_BROWSER_NOT_SUPPORTING_WEBAPKS,
                HostBrowserUtils.computeHostBrowserPackageClearCachedDataOnChange(mContext));
    }

    /**
     * Tests the order of precendence for {@link #getCachedHostBrowserPackage()}. Null should be
     * returned unless the shared preference points to an installed browser.
     */
    @Test
    public void testGetCachedHostBrowserPrecedence() {
        if (mIsBoundWebApk) {
            // Bound browser is installed.
            setHostBrowserInMetadata(BROWSERS_SUPPORTING_WEBAPKS[0]);
        }

        // Shared pref browser: Still installed
        {
            final String sharedPrefBrowserSupportingWebApks = BROWSERS_SUPPORTING_WEBAPKS[1];
            setInstalledBrowsersAndClearedCachedData(DefaultBrowserWebApkSupport.YES, ALL_BROWSERS);
            setHostBrowserInSharedPreferences(sharedPrefBrowserSupportingWebApks);
            Assert.assertEquals(sharedPrefBrowserSupportingWebApks,
                    HostBrowserUtils.getCachedHostBrowserPackage(mContext));
        }

        // Shared pref browser: Null
        // Should ignore bound browser and default browser.
        setInstalledBrowsersAndClearedCachedData(DefaultBrowserWebApkSupport.YES, ALL_BROWSERS);
        setHostBrowserInSharedPreferences(null);
        Assert.assertNull(HostBrowserUtils.getCachedHostBrowserPackage(mContext));
    }

    /**
     * Tests that neither {@link #computeHostBrowserPackageClearCachedDataOnChange()} nor
     * {@link #getCachedHostBrowserPackage()} return the cached browser in
     * {@link HostBrowserUtils#sHostPackage} if the cached browser was uninstalled.
     */
    @Test
    public void testDoesNotReturnTheCurrentHostBrowserAfterUninstall() {
        if (mIsBoundWebApk) {
            setHostBrowserInMetadata(BROWSERS_SUPPORTING_WEBAPKS[0]);
        }

        final String sharedPrefBrowserSupportingWebApks = BROWSERS_SUPPORTING_WEBAPKS[0];
        setInstalledBrowsersAndClearedCachedData(DefaultBrowserWebApkSupport.NO, ALL_BROWSERS);
        setHostBrowserInSharedPreferences(sharedPrefBrowserSupportingWebApks);
        Assert.assertEquals(sharedPrefBrowserSupportingWebApks,
                HostBrowserUtils.getCachedHostBrowserPackage(mContext));
        Assert.assertEquals(sharedPrefBrowserSupportingWebApks,
                HostBrowserUtils.computeHostBrowserPackageClearCachedDataOnChange(mContext));

        mBrowserInstaller.uninstallBrowser(sharedPrefBrowserSupportingWebApks);
        Assert.assertNotEquals(sharedPrefBrowserSupportingWebApks,
                HostBrowserUtils.getCachedHostBrowserPackage(mContext));
        Assert.assertNotEquals(sharedPrefBrowserSupportingWebApks,
                HostBrowserUtils.computeHostBrowserPackageClearCachedDataOnChange(mContext));
    }

    /**
     * Test that {@link #computeHostBrowserPackageClearCachedDataOnChange()} clears shared
     * preferences when its return value changes.
     */
    @Test
    public void testComputeHostBrowserClearsSharedPreferencesWhenReturnValueChanges() {
        if (mIsBoundWebApk) return;

        final String sharedPrefToTest = "to_test";
        SharedPreferences.Editor editor = mSharedPrefs.edit();
        editor.putString(sharedPrefToTest, "random");
        editor.apply();

        final String sharedPrefBrowserSupportingWebApks = BROWSERS_SUPPORTING_WEBAPKS[0];
        setInstalledBrowsersAndClearedCachedData(DefaultBrowserWebApkSupport.NO, ALL_BROWSERS);
        setHostBrowserInSharedPreferences(sharedPrefBrowserSupportingWebApks);

        Assert.assertEquals(sharedPrefBrowserSupportingWebApks,
                HostBrowserUtils.computeHostBrowserPackageClearCachedDataOnChange(mContext));
        Assert.assertEquals("random", mSharedPrefs.getString(sharedPrefToTest, null));

        mBrowserInstaller.uninstallBrowser(sharedPrefBrowserSupportingWebApks);
        Assert.assertNull(
                HostBrowserUtils.computeHostBrowserPackageClearCachedDataOnChange(mContext));

        // The return value of {@link computeHostBrowserPackageClearCachedDataOnChange} changed,
        // the shared preferences should have been cleared.
        Assert.assertNull(getHostBrowserFromSharedPreferences());
        Assert.assertNull(mSharedPrefs.getString(sharedPrefToTest, null));
    }

    /**
     * Tests that {@link#getCachedHostBrowserPackage()} does not modify shared preferences when its
     * return value changes.
     */
    @Test
    public void testGetCachedHostPackageDoesNotAffectSharedPreferences() {
        if (mIsBoundWebApk) return;

        final String sharedPrefToTest = "to_test";
        SharedPreferences.Editor editor = mSharedPrefs.edit();
        editor.putString(sharedPrefToTest, "random");
        editor.apply();

        final String sharedPrefBrowserSupportingWebApks = BROWSERS_SUPPORTING_WEBAPKS[0];
        setInstalledBrowsersAndClearedCachedData(DefaultBrowserWebApkSupport.NO, ALL_BROWSERS);
        setHostBrowserInSharedPreferences(sharedPrefBrowserSupportingWebApks);

        Assert.assertEquals(sharedPrefBrowserSupportingWebApks,
                HostBrowserUtils.getCachedHostBrowserPackage(mContext));

        mBrowserInstaller.uninstallBrowser(sharedPrefBrowserSupportingWebApks);
        Assert.assertNull(HostBrowserUtils.getCachedHostBrowserPackage(mContext));

        // SharedPreferences should be unaffected.
        Assert.assertEquals(
                sharedPrefBrowserSupportingWebApks, getHostBrowserFromSharedPreferences());
        Assert.assertEquals("random", mSharedPrefs.getString(sharedPrefToTest, null));
    }

    @SafeVarargs
    private static String[] mergeStringArrays(String[]... toMerge) {
        List<String> out = new ArrayList<String>();
        for (String[] toMergeArray : toMerge) {
            out.addAll(Arrays.asList(toMergeArray));
        }
        return out.toArray(new String[0]);
    }

    /**
     * Changes the installed browsers to the passed-in list and clears {@link HostBrowserUtils}
     * statics.
     */
    private void setInstalledBrowsersAndClearedCachedData(
            @DefaultBrowserWebApkSupport int defaultBrowser, String[] newPackages) {
        HostBrowserUtils.resetCachedHostPackageForTesting();
        String defaultPackage = (defaultBrowser == DefaultBrowserWebApkSupport.YES)
                ? DEFAULT_BROWSER_SUPPORTING_WEBAPKS
                : DEFAULT_BROWSER_NOT_SUPPORTING_WEBAPKS;
        mBrowserInstaller.setInstalledModernBrowsers(defaultPackage, newPackages);
    }

    private String getHostBrowserFromSharedPreferences() {
        return mSharedPrefs.getString(WebApkSharedPreferences.PREF_RUNTIME_HOST, null);
    }

    private void setHostBrowserInSharedPreferences(String hostBrowserPackage) {
        SharedPreferences.Editor editor = mSharedPrefs.edit();
        editor.putString(WebApkSharedPreferences.PREF_RUNTIME_HOST, hostBrowserPackage);
        editor.apply();
    }

    private void setHostBrowserInMetadata(String hostBrowserPackage) {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, hostBrowserPackage);
        WebApkTestHelper.registerWebApkWithMetaData(
                mContext.getPackageName(), bundle, null /* shareTargetMetaData */);
    }
}
