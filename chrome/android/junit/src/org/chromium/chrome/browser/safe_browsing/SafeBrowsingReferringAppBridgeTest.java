// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.safe_browsing;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingReferringAppBridge.ReferringAppInfo;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingReferringAppBridge.ReferringAppInfo.ReferringAppSource;
import org.chromium.chrome.test.util.browser.webapps.WebApkIntentDataProviderBuilder;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for SafeBrowsingReferringAppBridge. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SafeBrowsingReferringAppBridgeTest {
    @Mock private WindowAndroid mWindowAndroid;

    @Mock private ChromeActivity mActivity;

    private WeakReference<Activity> mActivityRef;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        mActivityRef = new WeakReference<>(mActivity);
        when(mWindowAndroid.getActivity()).thenReturn(mActivityRef);
    }

    @Test
    public void testFromKnownAppId() {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, IntentHandler.PACKAGE_GSA);
        when(mActivity.getIntent()).thenReturn(intent);

        ReferringAppInfo info =
                SafeBrowsingReferringAppBridge.getReferringAppInfo(mWindowAndroid, false);

        Assert.assertEquals(ReferringAppSource.KNOWN_APP_ID, info.getSource());
        Assert.assertEquals("google.search.app", info.getName());
        Assert.assertEquals("", info.getReferringWebApkStartUrl());
        Assert.assertEquals("", info.getReferringWebApkManifestId());
    }

    @Test
    public void testFromUnknownAppId() {
        String packageName = "uncommon.app.name";
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, packageName);
        when(mActivity.getIntent()).thenReturn(intent);

        ReferringAppInfo info =
                SafeBrowsingReferringAppBridge.getReferringAppInfo(mWindowAndroid, false);

        Assert.assertEquals(ReferringAppSource.UNKNOWN_APP_ID, info.getSource());
        Assert.assertEquals(packageName, info.getName());
        Assert.assertEquals("", info.getReferringWebApkStartUrl());
        Assert.assertEquals("", info.getReferringWebApkManifestId());
    }

    @Test
    public void testFromIntentExtraActivityReferrerHighVersion() {
        String appReferrer = "android-app://app.name/";
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.putExtra(IntentHandler.EXTRA_ACTIVITY_REFERRER, appReferrer);
        when(mActivity.getIntent()).thenReturn(intent);

        ReferringAppInfo info =
                SafeBrowsingReferringAppBridge.getReferringAppInfo(mWindowAndroid, false);

        Assert.assertEquals(ReferringAppSource.ACTIVITY_REFERRER, info.getSource());
        Assert.assertEquals(appReferrer, info.getName());
        Assert.assertEquals("", info.getReferringWebApkStartUrl());
        Assert.assertEquals("", info.getReferringWebApkManifestId());
    }

    @Test
    public void testFromActivityReferrerHighVersion() {
        String appReferrer = "android-app://app.name/";
        setAppReferrerIntent(appReferrer);

        ReferringAppInfo info =
                SafeBrowsingReferringAppBridge.getReferringAppInfo(mWindowAndroid, false);

        Assert.assertEquals(ReferringAppSource.ACTIVITY_REFERRER, info.getSource());
        Assert.assertEquals(appReferrer, info.getName());
        Assert.assertEquals("", info.getReferringWebApkStartUrl());
        Assert.assertEquals("", info.getReferringWebApkManifestId());
    }

    @Test
    public void testGetWebApkInfo() {
        final String webApkPackageName = "org.chromium.webapk.foo";
        final String webApkStartUrl = "https://example.test/app";
        final String webApkManifestId = "https://example.test/id";
        // Set up the WebAPK referrer.
        BaseCustomTabActivity mockCustomTabActivity = mock(BaseCustomTabActivity.class);
        when(mockCustomTabActivity.getIntentDataProvider())
                .thenReturn(
                        new WebApkIntentDataProviderBuilder(webApkPackageName, webApkStartUrl)
                                .setWebApkManifestId(webApkManifestId)
                                .build());
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mockCustomTabActivity));
        // Add a previous app referrer to the Intent, to test that both the previous referrer and
        // the WebAPK referrer are captured.
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, IntentHandler.PACKAGE_GSA);
        when(mockCustomTabActivity.getIntent()).thenReturn(intent);

        // Check that when WebAPK info is not explicitly requested, the fields are not populated.
        ReferringAppInfo infoWithoutWebApk =
                SafeBrowsingReferringAppBridge.getReferringAppInfo(mWindowAndroid, false);
        Assert.assertEquals(ReferringAppSource.KNOWN_APP_ID, infoWithoutWebApk.getSource());
        Assert.assertEquals("google.search.app", infoWithoutWebApk.getName());
        Assert.assertEquals("", infoWithoutWebApk.getReferringWebApkStartUrl());
        Assert.assertEquals("", infoWithoutWebApk.getReferringWebApkManifestId());

        // Now get the WebAPK info. The previous app referrer info should also be present.
        ReferringAppInfo info =
                SafeBrowsingReferringAppBridge.getReferringAppInfo(mWindowAndroid, true);

        Assert.assertEquals(ReferringAppSource.KNOWN_APP_ID, info.getSource());
        Assert.assertEquals("google.search.app", info.getName());
        Assert.assertEquals(webApkStartUrl, info.getReferringWebApkStartUrl());
        Assert.assertEquals(webApkManifestId, info.getReferringWebApkManifestId());
    }

    private void setAppReferrerIntent(String appReferrer) {
        Uri appReferrerUri = Uri.parse(appReferrer);
        Bundle extras = new Bundle();
        extras.putParcelable(Intent.EXTRA_REFERRER, appReferrerUri);
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.putExtras(extras);
        when(mActivity.getIntent()).thenReturn(intent);
        when(mActivity.getReferrer()).thenReturn(appReferrerUri);
    }
}
