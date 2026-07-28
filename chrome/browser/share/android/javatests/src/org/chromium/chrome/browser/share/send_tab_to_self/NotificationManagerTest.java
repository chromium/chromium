// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;

/** Tests for NotificationManager */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_OPEN_NATIVE_APP)
public class NotificationManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SendTabToSelfAndroidBridge.Natives mBridgeMock;
    @Mock private SendTabToSelfMetricsRecorder.Natives mMetricsMock;
    @Mock private Profile mProfile;

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.getApplication());
        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mBridgeMock);
        SendTabToSelfMetricsRecorderJni.setInstanceForTesting(mMetricsMock);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
    }

    @Test
    @SmallTest
    public void testNotificationTap() {
        String guid = "test_guid";
        String url = "https://www.example.com";
        Intent intent = new Intent("send_tab_to_self.tap");
        intent.putExtra("send_tab_to_self.notification.guid", guid);
        intent.setData(android.net.Uri.parse(url));

        // Add active notification.
        NotificationSharedPrefManager.addActiveNotification(
                new NotificationSharedPrefManager.ActiveNotification(1, guid));

        NotificationManager.handleIntent(intent);

        // Verify that it marked it as opened and activated.
        verify(mBridgeMock).markEntryOpened(any(), eq(guid));
        verify(mBridgeMock)
                .markEntryActivated(
                        any(), eq(guid), eq(ShareActivatedEntryPoint.MOBILE_NOTIFICATION));
        verify(mMetricsMock).recordNotificationOpened();

        // Verify that the started intent is explicit and targets Chrome.
        Intent startedIntent =
                Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity();
        Assert.assertNotNull(startedIntent);
        Assert.assertEquals(Intent.ACTION_VIEW, startedIntent.getAction());
        Assert.assertEquals(url, startedIntent.getDataString());
        Assert.assertEquals(
                "org.chromium.chrome.browser.document.ChromeLauncherActivity",
                startedIntent.getComponent().getClassName());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_OPEN_NATIVE_APP)
    public void testNotificationTapWithSpecializedHandler() {
        String guid = "test_guid";
        String url = "https://www.example.com/app/path";
        Intent intent = new Intent("send_tab_to_self.tap");
        intent.putExtra("send_tab_to_self.notification.guid", guid);
        intent.setData(android.net.Uri.parse(url));

        // Add active notification.
        NotificationSharedPrefManager.addActiveNotification(
                new NotificationSharedPrefManager.ActiveNotification(1, guid));

        // Register a specialized handler.
        ShadowPackageManager shadowPackageManager =
                Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager());

        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addDataScheme("https");
        filter.addDataAuthority("www.example.com", null);
        filter.addDataPath("/app", android.os.PatternMatcher.PATTERN_PREFIX);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);

        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.packageName = "com.example.app";
        resolveInfo.activityInfo.name = "com.example.app.MainActivity";
        resolveInfo.filter = filter;

        Intent queryIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        queryIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        shadowPackageManager.addResolveInfoForIntent(queryIntent, resolveInfo);

        NotificationManager.handleIntent(intent);

        // Verify that it marked it as opened and activated.
        verify(mBridgeMock).markEntryOpened(any(), eq(guid));
        verify(mBridgeMock)
                .markEntryActivated(
                        any(), eq(guid), eq(ShareActivatedEntryPoint.MOBILE_NOTIFICATION));
        verify(mMetricsMock).recordNotificationOpened();

        // Verify that the started intent is implicit and doesn't target Chrome.
        Intent startedIntent =
                Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity();
        Assert.assertNotNull(startedIntent);
        Assert.assertEquals(Intent.ACTION_VIEW, startedIntent.getAction());
        Assert.assertEquals(url, startedIntent.getDataString());
        Assert.assertNull(startedIntent.getComponent());
        Assert.assertNull(startedIntent.getPackage());
        Assert.assertTrue(startedIntent.getCategories().contains(Intent.CATEGORY_BROWSABLE));
    }

    @Test
    @SmallTest
    public void testNotificationTapWithSpecializedHandlerButFeatureDisabled() {
        String guid = "test_guid";
        String url = "https://www.example.com/app/path";
        Intent intent = new Intent("send_tab_to_self.tap");
        intent.putExtra("send_tab_to_self.notification.guid", guid);
        intent.setData(android.net.Uri.parse(url));

        // Add active notification.
        NotificationSharedPrefManager.addActiveNotification(
                new NotificationSharedPrefManager.ActiveNotification(1, guid));

        // Register a specialized handler.
        ShadowPackageManager shadowPackageManager =
                Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager());

        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addDataScheme("https");
        filter.addDataAuthority("www.example.com", null);
        filter.addDataPath("/app", android.os.PatternMatcher.PATTERN_PREFIX);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);

        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.packageName = "com.example.app";
        resolveInfo.activityInfo.name = "com.example.app.MainActivity";
        resolveInfo.filter = filter;

        Intent queryIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        queryIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        shadowPackageManager.addResolveInfoForIntent(queryIntent, resolveInfo);

        NotificationManager.handleIntent(intent);

        // Verify that it marked it as opened and activated.
        verify(mBridgeMock).markEntryOpened(any(), eq(guid));
        verify(mBridgeMock)
                .markEntryActivated(
                        any(), eq(guid), eq(ShareActivatedEntryPoint.MOBILE_NOTIFICATION));
        verify(mMetricsMock).recordNotificationOpened();

        // Verify that the started intent targets Chrome (because feature is disabled by default).
        Intent startedIntent =
                Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity();
        Assert.assertNotNull(startedIntent);
        Assert.assertEquals(Intent.ACTION_VIEW, startedIntent.getAction());
        Assert.assertEquals(url, startedIntent.getDataString());
        Assert.assertEquals(
                "org.chromium.chrome.browser.document.ChromeLauncherActivity",
                startedIntent.getComponent().getClassName());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_OPEN_NATIVE_APP)
    public void testOpenInNativeAppIfPossibleWithSpecializedHandler() {
        String url = "https://www.example.com/app/path";

        // Register a specialized handler.
        ShadowPackageManager shadowPackageManager =
                Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager());

        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addDataScheme("https");
        filter.addDataAuthority("www.example.com", null);
        filter.addDataPath("/app", android.os.PatternMatcher.PATTERN_PREFIX);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);

        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.packageName = "com.example.app";
        resolveInfo.activityInfo.name = "com.example.app.MainActivity";
        resolveInfo.filter = filter;

        Intent queryIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        queryIntent.addCategory(Intent.CATEGORY_BROWSABLE);
        shadowPackageManager.addResolveInfoForIntent(queryIntent, resolveInfo);

        boolean result = NotificationManager.openInNativeAppIfPossible(url);
        Assert.assertTrue(result);

        // Verify that the started intent is implicit and doesn't target Chrome.
        Intent startedIntent =
                Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity();
        Assert.assertNotNull(startedIntent);
        Assert.assertEquals(Intent.ACTION_VIEW, startedIntent.getAction());
        Assert.assertEquals(url, startedIntent.getDataString());
        Assert.assertNull(startedIntent.getComponent());
        Assert.assertNull(startedIntent.getPackage());
        Assert.assertTrue(startedIntent.getCategories().contains(Intent.CATEGORY_BROWSABLE));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_OPEN_NATIVE_APP)
    public void testOpenInNativeAppIfPossibleNoSpecializedHandler() {
        String url = "https://www.example.com/app/path";
        // Do not register specialized handler.

        boolean result = NotificationManager.openInNativeAppIfPossible(url);
        Assert.assertFalse(result);

        Intent startedIntent =
                Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity();
        Assert.assertNull(startedIntent);
    }
}
