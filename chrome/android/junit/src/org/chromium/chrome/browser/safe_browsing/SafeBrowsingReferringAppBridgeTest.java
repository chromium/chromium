// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.safe_browsing;

import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingReferringAppBridge.ReferringAppInfo;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingReferringAppBridge.ReferringAppInfo.ReferringAppSource;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for SafeBrowsingReferringAppBridge. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SafeBrowsingReferringAppBridgeTest {
    @Mock private WindowAndroid mWindowAndroid;

    @Mock private ChromeActivity mActivity;

    private WeakReference<Activity> mActivityRef;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityRef = new WeakReference<>(mActivity);

        when(mWindowAndroid.getActivity()).thenReturn(mActivityRef);
    }

    @Test
    public void testFromKnownAppId() {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, IntentHandler.PACKAGE_GSA);
        when(mActivity.getIntent()).thenReturn(intent);

        ReferringAppInfo info = SafeBrowsingReferringAppBridge.getReferringAppInfo(mWindowAndroid);

        Assert.assertEquals(ReferringAppSource.KNOWN_APP_ID, info.getSource());
        Assert.assertEquals("google.search.app", info.getName());
    }

    @Test
    public void testFromUnknownAppId() {
        String packageName = "uncommon.app.name";
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, packageName);
        when(mActivity.getIntent()).thenReturn(intent);

        ReferringAppInfo info = SafeBrowsingReferringAppBridge.getReferringAppInfo(mWindowAndroid);

        Assert.assertEquals(ReferringAppSource.UNKNOWN_APP_ID, info.getSource());
        Assert.assertEquals(packageName, info.getName());
    }

    @Test
    public void testFromIntentExtraActivityReferrerHighVersion() {
        String appReferrer = "android-app://app.name/";
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.putExtra(IntentHandler.EXTRA_ACTIVITY_REFERRER, appReferrer);
        when(mActivity.getIntent()).thenReturn(intent);

        ReferringAppInfo info = SafeBrowsingReferringAppBridge.getReferringAppInfo(mWindowAndroid);

        Assert.assertEquals(ReferringAppSource.ACTIVITY_REFERRER, info.getSource());
        Assert.assertEquals(appReferrer, info.getName());
    }

    @Test
    public void testFromActivityReferrerHighVersion() {
        String appReferrer = "android-app://app.name/";
        setAppReferrerIntent(appReferrer);

        ReferringAppInfo info = SafeBrowsingReferringAppBridge.getReferringAppInfo(mWindowAndroid);

        Assert.assertEquals(ReferringAppSource.ACTIVITY_REFERRER, info.getSource());
        Assert.assertEquals(appReferrer, info.getName());
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
