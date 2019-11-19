// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.os.Build;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.instantapps.InstantAppsHandler;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Instrumentation tests for {@link ExternalNavigationHandler}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ExternalNavigationDelegateImplTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static List<ResolveInfo> makeResolveInfos(ResolveInfo... infos) {
        return Arrays.asList(infos);
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_NoResolveInfo() {
        String packageName = "";
        List<ResolveInfo> resolveInfos = new ArrayList<ResolveInfo>();
        Assert.assertEquals(0,
                ExternalNavigationDelegateImpl
                        .getSpecializedHandlersWithFilter(resolveInfos, packageName)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_NoPathOrAuthority() {
        String packageName = "";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        Assert.assertEquals(0,
                ExternalNavigationDelegateImpl
                        .getSpecializedHandlersWithFilter(resolveInfos, packageName)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_WithPath() {
        String packageName = "";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataPath("somepath", 2);
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        Assert.assertEquals(1,
                ExternalNavigationDelegateImpl
                        .getSpecializedHandlersWithFilter(resolveInfos, packageName)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_WithAuthority() {
        String packageName = "";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataAuthority("http://www.google.com", "80");
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        Assert.assertEquals(1,
                ExternalNavigationDelegateImpl
                        .getSpecializedHandlersWithFilter(resolveInfos, packageName)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_WithAuthority_Wildcard_Host() {
        String packageName = "";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataAuthority("*", null);
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        Assert.assertEquals(0,
                ExternalNavigationDelegateImpl
                        .getSpecializedHandlersWithFilter(resolveInfos, packageName)
                        .size());

        ResolveInfo infoWildcardSubDomain = new ResolveInfo();
        infoWildcardSubDomain.filter = new IntentFilter();
        infoWildcardSubDomain.filter.addDataAuthority("http://*.google.com", "80");
        List<ResolveInfo> resolveInfosWildcardSubDomain = makeResolveInfos(infoWildcardSubDomain);
        Assert.assertEquals(1,
                ExternalNavigationDelegateImpl
                        .getSpecializedHandlersWithFilter(
                                resolveInfosWildcardSubDomain, packageName)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_WithTargetPackage_Matching() {
        String packageName = "com.android.chrome";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataAuthority("http://www.google.com", "80");
        info.activityInfo = new ActivityInfo();
        info.activityInfo.packageName = packageName;
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        Assert.assertEquals(1,
                ExternalNavigationDelegateImpl
                        .getSpecializedHandlersWithFilter(resolveInfos, packageName)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializedHandler_WithTargetPackage_NotMatching() {
        String packageName = "com.android.chrome";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataAuthority("http://www.google.com", "80");
        info.activityInfo = new ActivityInfo();
        info.activityInfo.packageName = "com.foo.bar";
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        Assert.assertEquals(0,
                ExternalNavigationDelegateImpl
                        .getSpecializedHandlersWithFilter(resolveInfos, packageName)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsPackageSpecializeHandler_withEphemeralResolver() {
        String packageName = "";
        ResolveInfo info = new ResolveInfo();
        info.filter = new IntentFilter();
        info.filter.addDataPath("somepath", 2);
        info.activityInfo = new ActivityInfo();

        // See InstantAppsHandler.isInstantAppResolveInfo
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            info.isInstantAppAvailable = true;
        } else {
            info.activityInfo.name = InstantAppsHandler.EPHEMERAL_INSTALLER_CLASS;
        }
        info.activityInfo.packageName = "com.google.android.gms";
        List<ResolveInfo> resolveInfos = makeResolveInfos(info);
        // Ephemeral resolver is not counted as a specialized handler.
        Assert.assertEquals(0,
                ExternalNavigationDelegateImpl
                        .getSpecializedHandlersWithFilter(resolveInfos, packageName)
                        .size());
    }

    @Test
    @SmallTest
    public void testIsDownload_noSystemDownloadManager() {
        ExternalNavigationDelegateImpl delegate = new ExternalNavigationDelegateImpl(
                mActivityTestRule.getActivity().getActivityTab());
        Assert.assertTrue("pdf should be a download, no viewer in Android Chrome",
                delegate.isPdfDownload("http://somesampeleurldne.com/file.pdf"));
        Assert.assertFalse("URL is not a file, but web page",
                delegate.isPdfDownload("http://somesampleurldne.com/index.html"));
        Assert.assertFalse("URL is not a file url",
                delegate.isPdfDownload("http://somesampeleurldne.com/not.a.real.extension"));
        Assert.assertFalse("URL is an image, can be viewed in Chrome",
                delegate.isPdfDownload("http://somesampleurldne.com/image.jpg"));
        Assert.assertFalse("URL is a text file can be viewed in Chrome",
                delegate.isPdfDownload("http://somesampleurldne.com/copy.txt"));
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }
}
