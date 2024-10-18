// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
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

import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.shell_apk.HostBrowserUtils.PackageNameAndComponentName;
import org.chromium.webapk.test.WebApkTestHelper;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;

/** Tests for HostBrowserUtils. */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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
            mergeStringArrays(
                    new String[] {
                        DEFAULT_BROWSER_SUPPORTING_WEBAPKS, DEFAULT_BROWSER_NOT_SUPPORTING_WEBAPKS
                    },
                    BROWSERS_SUPPORTING_WEBAPKS,
                    BROWSERS_NOT_SUPPORTING_WEBAPKS);

    private Context mContext;
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
    }

    /**
     * Tests that {@link #computeHostBrowserPackageName()} returns null if there isn't any browser
     * installed on the device.
     */
    @Test
    public void testReturnsNullWhenNoBrowserInstalled() {
        if (mIsBoundWebApk) {
            // Bound browser in AndroidManifest.xml is no longer installed.
            setHostBrowserInMetadata(BROWSERS_SUPPORTING_WEBAPKS[0]);
        }
        Assert.assertNull(HostBrowserUtils.computeHostBrowserPackageNameAndComponentName(mContext));
    }

    // Tests the order of precedence for bound WebAPKs for
    // {@link #computeHostBrowserPackageName()}. The expected order of precedence
    // is:
    // 1) Bound browser specified in AndroidManifest.xml
    // 2) The default browser.
    @Test
    public void testComputeHostBrowserBoundWebApkPrecedence() {
        if (!mIsBoundWebApk) return;

        final String boundBrowserSupportingWebApks = BROWSERS_SUPPORTING_WEBAPKS[0];
        setHostBrowserInMetadata(boundBrowserSupportingWebApks);

        // Bound browser in AndroidManifest.xml: Still installed
        // No ComponentName is present for bound WebAPKs.
        setInstalledBrowsers(DefaultBrowserWebApkSupport.YES, ALL_BROWSERS);
        PackageNameAndComponentName packageAndComponent =
                HostBrowserUtils.computeHostBrowserPackageNameAndComponentName(mContext);
        Assert.assertEquals(boundBrowserSupportingWebApks, packageAndComponent.getPackageName());
        Assert.assertNull(packageAndComponent.getComponentName());

        // Bound browser in AndroidManifest.xml: No longer installed
        // Should use default browser.
        // ComponentName exists because this is effectively an unbound WebAPK now.
        setInstalledBrowsers(DefaultBrowserWebApkSupport.YES, null);
        packageAndComponent =
                HostBrowserUtils.computeHostBrowserPackageNameAndComponentName(mContext);
        Assert.assertEquals(
                DEFAULT_BROWSER_SUPPORTING_WEBAPKS, packageAndComponent.getPackageName());
        Assert.assertNotNull(packageAndComponent.getComponentName());
        Assert.assertEquals(
                DEFAULT_BROWSER_SUPPORTING_WEBAPKS,
                packageAndComponent.getComponentName().getPackageName());
        Assert.assertEquals(
                TestBrowserInstaller.COMPONENT_CLASS,
                packageAndComponent.getComponentName().getClassName());
    }

    // Tests the order of precedence for unbound WebAPKs for
    // {@link #computeHostBrowserPackageName()}. This is expected to always open in the default
    // browser, and the ComponentName should always exist.
    @Test
    public void testComputeHostBrowserUnboundWebApkPrecedence() {
        if (mIsBoundWebApk) return;

        // Default browser: Supports WebAPKs
        setInstalledBrowsers(DefaultBrowserWebApkSupport.YES, ALL_BROWSERS);
        PackageNameAndComponentName packageAndComponent =
                HostBrowserUtils.computeHostBrowserPackageNameAndComponentName(mContext);
        Assert.assertEquals(
                DEFAULT_BROWSER_SUPPORTING_WEBAPKS, packageAndComponent.getPackageName());
        Assert.assertNotNull(packageAndComponent.getComponentName());
        Assert.assertEquals(
                DEFAULT_BROWSER_SUPPORTING_WEBAPKS,
                packageAndComponent.getComponentName().getPackageName());
        Assert.assertEquals(
                TestBrowserInstaller.COMPONENT_CLASS,
                packageAndComponent.getComponentName().getClassName());

        // Default browser: Does not support WebAPKs
        // > 1 installed browsers supporting WebAPKs - just use the default browser even though it
        // doesn't support WebAPKs.
        setInstalledBrowsers(DefaultBrowserWebApkSupport.NO, ALL_BROWSERS);
        packageAndComponent =
                HostBrowserUtils.computeHostBrowserPackageNameAndComponentName(mContext);
        Assert.assertEquals(
                DEFAULT_BROWSER_NOT_SUPPORTING_WEBAPKS, packageAndComponent.getPackageName());
        Assert.assertNotNull(packageAndComponent.getComponentName());
        Assert.assertEquals(
                DEFAULT_BROWSER_NOT_SUPPORTING_WEBAPKS,
                packageAndComponent.getComponentName().getPackageName());
        Assert.assertEquals(
                TestBrowserInstaller.COMPONENT_CLASS,
                packageAndComponent.getComponentName().getClassName());

        // Default browser: Does not support WebAPKS
        // 1 installed browser supporting WebAPKS - still use the default browser even though it
        // doesn't support WebAPKs.
        setInstalledBrowsers(
                DefaultBrowserWebApkSupport.NO,
                new String[] {
                    BROWSERS_SUPPORTING_WEBAPKS[0],
                    BROWSERS_NOT_SUPPORTING_WEBAPKS[0],
                    BROWSERS_NOT_SUPPORTING_WEBAPKS[1]
                });
        packageAndComponent =
                HostBrowserUtils.computeHostBrowserPackageNameAndComponentName(mContext);
        Assert.assertEquals(
                DEFAULT_BROWSER_NOT_SUPPORTING_WEBAPKS, packageAndComponent.getPackageName());
        Assert.assertNotNull(packageAndComponent.getComponentName());
        Assert.assertEquals(
                DEFAULT_BROWSER_NOT_SUPPORTING_WEBAPKS,
                packageAndComponent.getComponentName().getPackageName());
        Assert.assertEquals(
                TestBrowserInstaller.COMPONENT_CLASS,
                packageAndComponent.getComponentName().getClassName());

        // Default browser: Does not support WebAPKs
        // > 1 installed browsers
        setInstalledBrowsers(
                DefaultBrowserWebApkSupport.NO,
                new String[] {
                    BROWSERS_NOT_SUPPORTING_WEBAPKS[0], BROWSERS_NOT_SUPPORTING_WEBAPKS[1]
                });
        packageAndComponent =
                HostBrowserUtils.computeHostBrowserPackageNameAndComponentName(mContext);
        Assert.assertEquals(
                DEFAULT_BROWSER_NOT_SUPPORTING_WEBAPKS, packageAndComponent.getPackageName());
        Assert.assertNotNull(packageAndComponent.getComponentName());
        Assert.assertEquals(
                DEFAULT_BROWSER_NOT_SUPPORTING_WEBAPKS,
                packageAndComponent.getComponentName().getPackageName());
        Assert.assertEquals(
                TestBrowserInstaller.COMPONENT_CLASS,
                packageAndComponent.getComponentName().getClassName());
    }

    /**
     * Tests that {@link #computeHostBrowserPackageName()} does not return a browser that has been
     * uninstalled.
     */
    @Test
    public void testDoesNotReturnTheCurrentHostBrowserAfterUninstall() {
        String browserToUse = DEFAULT_BROWSER_NOT_SUPPORTING_WEBAPKS;
        if (mIsBoundWebApk) {
            browserToUse = BROWSERS_SUPPORTING_WEBAPKS[0];
            setHostBrowserInMetadata(browserToUse);
        }

        setInstalledBrowsers(DefaultBrowserWebApkSupport.NO, ALL_BROWSERS);
        PackageNameAndComponentName packageAndComponent =
                HostBrowserUtils.computeHostBrowserPackageNameAndComponentName(mContext);
        Assert.assertEquals(browserToUse, packageAndComponent.getPackageName());
        if (mIsBoundWebApk) {
            Assert.assertNull(packageAndComponent.getComponentName());
        } else {
            Assert.assertNotNull(packageAndComponent.getComponentName());
            Assert.assertEquals(
                    browserToUse, packageAndComponent.getComponentName().getPackageName());
            Assert.assertEquals(
                    TestBrowserInstaller.COMPONENT_CLASS,
                    packageAndComponent.getComponentName().getClassName());
        }

        mBrowserInstaller.uninstallBrowser(browserToUse);
        packageAndComponent =
                HostBrowserUtils.computeHostBrowserPackageNameAndComponentName(mContext);
        Assert.assertNotEquals(browserToUse, packageAndComponent.getPackageName());
        Assert.assertNotNull(packageAndComponent.getComponentName());
        Assert.assertNotEquals(
                browserToUse, packageAndComponent.getComponentName().getPackageName());
        Assert.assertEquals(
                TestBrowserInstaller.COMPONENT_CLASS,
                packageAndComponent.getComponentName().getClassName());
    }

    @SafeVarargs
    private static String[] mergeStringArrays(String[]... toMerge) {
        List<String> out = new ArrayList<String>();
        for (String[] toMergeArray : toMerge) {
            out.addAll(Arrays.asList(toMergeArray));
        }
        return out.toArray(new String[0]);
    }

    /** Changes the installed browsers to the passed-in list. */
    private void setInstalledBrowsers(
            @DefaultBrowserWebApkSupport int defaultBrowser, String[] newPackages) {
        String defaultPackage =
                (defaultBrowser == DefaultBrowserWebApkSupport.YES)
                        ? DEFAULT_BROWSER_SUPPORTING_WEBAPKS
                        : DEFAULT_BROWSER_NOT_SUPPORTING_WEBAPKS;
        mBrowserInstaller.setInstalledModernBrowsers(defaultPackage, newPackages);
    }

    private void setHostBrowserInMetadata(String hostBrowserPackage) {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, hostBrowserPackage);
        WebApkTestHelper.registerWebApkWithMetaData(
                mContext.getPackageName(), bundle, /* shareTargetMetaData= */ null);
    }
}
