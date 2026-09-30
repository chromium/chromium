// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Robolectric.setupActivity;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

import java.util.ArrayList;
import java.util.Arrays;

/** Tests for WebApkUtils. */
@RunWith(RobolectricTestRunner.class)
@SuppressWarnings("DoNotMock") // TODO(567604165): Remove mocking of Views / Activities
public class WebApkUtilsTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Context mMockApplicationContext;
    @Mock private PackageManager mMockPackageManager;

    protected static final String WEBAPK_PACKAGE_NAME = "org.chromium.test";

    private Context mContext;
    private ShadowPackageManager mPackageManager;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mPackageManager = Shadows.shadowOf(mContext.getPackageManager());
    }

    /**
     * Test that MainActivity appends the start URL as a paramater if |loggedIntentUrlParam| in
     * WebAPK metadata is set and {@link intentStartUrl} is outside of the scope specified in the
     * manifest meta data.
     */
    @Test
    public void testLoggedIntentUrlParamWhenRewriteOutOfScope() {
        final String intentStartUrl = "https://maps.google.com/page?a=A";
        final String manifestStartUrl = "https://www.google.com/maps";
        final String manifestScope = "https://www.google.com";
        final String expectedRewrittenStartUrl =
                "https://www.google.com/maps?originalUrl=https%3A%2F%2Fmaps.google.com%2Fpage%3Fa%3DA";
        final String browserPackageName = "browser.support.webapks";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, manifestStartUrl);
        bundle.putString(WebApkMetaDataKeys.SCOPE, manifestScope);
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, browserPackageName);
        bundle.putString(WebApkMetaDataKeys.LOGGED_INTENT_URL_PARAM, "originalUrl");

        Assert.assertEquals(
                expectedRewrittenStartUrl,
                WebApkUtils.rewriteIntentUrlIfNecessary(intentStartUrl, bundle));
    }

    /**
     * Test that MainActivity appends the start URL as a paramater if |loggedIntentUrlParam| in
     * WebAPK metadata is set and {@link intentStartUrl} is in the scope specified in the manifest
     * meta data.
     */
    @Test
    public void testLoggedIntentUrlParamWhenRewriteInScope() {
        final String intentStartUrl = "https://www.google.com/maps/search/A";
        final String manifestStartUrl = "https://www.google.com/maps?force=qVTs2FOxxTmHHo79-pwa";
        final String manifestScope = "https://www.google.com";
        final String expectedRewrittenStartUrl =
                "https://www.google.com/maps?force=qVTs2FOxxTmHHo79-pwa&intent="
                        + "https%3A%2F%2Fwww.google.com%2Fmaps%2Fsearch%2FA";
        final String browserPackageName = "browser.support.webapks";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, manifestStartUrl);
        bundle.putString(WebApkMetaDataKeys.SCOPE, manifestScope);
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, browserPackageName);
        bundle.putString(WebApkMetaDataKeys.LOGGED_INTENT_URL_PARAM, "intent");

        Assert.assertEquals(
                expectedRewrittenStartUrl,
                WebApkUtils.rewriteIntentUrlIfNecessary(intentStartUrl, bundle));
    }

    /**
     * Tests that {@link WebApkUtils#isInstalled} returns false for an installed but disabled app.
     */
    @Test
    public void testReturnFalseForInstalledButDisabledApp() {
        String packageName = "com.chrome.beta";
        PackageInfo info = new PackageInfo();
        info.packageName = packageName;
        info.applicationInfo = new ApplicationInfo();
        info.applicationInfo.enabled = false;
        mPackageManager.addPackage(info);

        Assert.assertFalse(WebApkUtils.isInstalled(mContext.getPackageManager(), packageName));
    }

    /** Test status bar is always black in Automotive devices. */
    @Test
    public void testStatusBarBlackInAutomotive() {
        // Create an "automotive" Activity.
        Activity testActivity = spy(setupActivity(Activity.class));
        doReturn(mMockApplicationContext).when(testActivity).getApplicationContext();
        when(mMockApplicationContext.getPackageManager()).thenReturn(mMockPackageManager);
        when(mMockPackageManager.hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE))
                .thenReturn(true);
        View rootView = testActivity.getWindow().getDecorView().getRootView();

        WebApkUtils.setStatusBarColor(testActivity, Color.RED);
        WebApkUtils.setStatusBarIconColor(rootView, false, testActivity);

        // No matter what color the status bar is being set on other form factors, it should always
        // be black on automotive devices.
        assertEquals(
                "Status bar should always be black in automotive devices.",
                Color.BLACK,
                testActivity.getWindow().getStatusBarColor());
        assertNotEquals(
                "Status bar should NOT use dark icons in automotive devices.",
                View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR,
                rootView.getSystemUiVisibility() & View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR);

        testActivity.finish();
    }

    /**
     * Tests that {@link WebApkUtils#grantUriPermissionToHostBrowserIfShare} catches {@link
     * SecurityException} when granting URI permission fails (e.g. when the sending app omits {@link
     * Intent#FLAG_GRANT_READ_URI_PERMISSION}) and continues granting permissions for remaining
     * URIs.
     */
    @Test
    public void testGrantUriPermissionToHostBrowserIfShareCatchesSecurityException() {
        Context spyContext = spy(mContext);
        Uri ungrantedUri = Uri.parse("content://com.example.provider/ungranted");
        Uri grantedUri = Uri.parse("content://com.example.provider/granted");
        doThrow(new SecurityException("No permission"))
                .when(spyContext)
                .grantUriPermission(
                        anyString(), eq(ungrantedUri), eq(Intent.FLAG_GRANT_READ_URI_PERMISSION));

        String shareActivityClassName =
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(0);
        Bundle metadata = new Bundle();
        metadata.putString(WebApkMetaDataKeys.START_URL, "https://pwa.rocks/");
        Bundle shareMetadata = new Bundle();
        shareMetadata.putString(WebApkMetaDataKeys.SHARE_ACTION, "https://pwa.rocks/share");
        shareMetadata.putString(WebApkMetaDataKeys.SHARE_METHOD, "POST");
        WebApkTestHelper.registerWebApkWithMetaData(
                mContext.getPackageName(), metadata, new Bundle[] {shareMetadata});

        // Test single ungranted URI in ACTION_SEND.
        Intent sendIntent = new Intent(Intent.ACTION_SEND);
        sendIntent.setComponent(
                new ComponentName(mContext.getPackageName(), shareActivityClassName));
        sendIntent.putExtra(Intent.EXTRA_STREAM, ungrantedUri);
        HostBrowserLauncherParams sendParams =
                HostBrowserLauncherParams.createForIntent(
                        spyContext,
                        sendIntent,
                        new HostBrowserUtils.PackageNameAndComponentName("com.chrome.beta"),
                        /* dialogShown= */ false,
                        /* launchTimeMs= */ -1,
                        /* splashShownTimeMs= */ -1);
        WebApkUtils.grantUriPermissionToHostBrowserIfShare(spyContext, sendParams);

        // Test multiple URIs in ACTION_SEND_MULTIPLE (including null and ungranted URI).
        Intent sendMultipleIntent = new Intent(Intent.ACTION_SEND_MULTIPLE);
        sendMultipleIntent.setComponent(
                new ComponentName(mContext.getPackageName(), shareActivityClassName));
        ArrayList<Uri> streamUris = new ArrayList<>(Arrays.asList(null, ungrantedUri, grantedUri));
        sendMultipleIntent.putParcelableArrayListExtra(Intent.EXTRA_STREAM, streamUris);
        HostBrowserLauncherParams sendMultipleParams =
                HostBrowserLauncherParams.createForIntent(
                        spyContext,
                        sendMultipleIntent,
                        new HostBrowserUtils.PackageNameAndComponentName("com.chrome.beta"),
                        /* dialogShown= */ false,
                        /* launchTimeMs= */ -1,
                        /* splashShownTimeMs= */ -1);
        WebApkUtils.grantUriPermissionToHostBrowserIfShare(spyContext, sendMultipleParams);

        verify(spyContext)
                .grantUriPermission(
                        eq("com.chrome.beta"),
                        eq(grantedUri),
                        eq(Intent.FLAG_GRANT_READ_URI_PERMISSION));
    }
}
