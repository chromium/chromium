// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
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
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationIntentInterceptor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationFeatureMap;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;

/** Tests for NotificationManager */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({
    ChromeFeatureList.SEND_TAB_TO_SELF_OPEN_NATIVE_APP,
    NotificationFeatureMap.CACHE_NOTIIFICATIONS_ENABLED
})
public class NotificationManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SendTabToSelfAndroidBridge.Natives mBridgeMock;
    @Mock private SendTabToSelfMetricsRecorder.Natives mMetricsMock;
    @Mock private BaseNotificationManagerProxy mNotificationManagerProxy;
    @Mock private Profile mProfile;
    private static final String GUID = "test_guid";
    private static final String URL = "https://www.example.com";

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.getApplication());
        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mBridgeMock);
        SendTabToSelfMetricsRecorderJni.setInstanceForTesting(mMetricsMock);
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mNotificationManagerProxy);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
    }

    @After
    public void tearDown() {
        BaseNotificationManagerProxyFactory.setInstanceForTesting(null);
    }


    private static Intent createTapIntent(
            String guid, String url, byte @Nullable [] pageContextBytes) {
        Intent intent = new Intent(NotificationManager.NOTIFICATION_ACTION_TAP);
        intent.putExtra(NotificationManager.NOTIFICATION_GUID_EXTRA, guid);
        intent.setData(Uri.parse(url));
        if (pageContextBytes != null) {
            intent.putExtra(IntentHandler.EXTRA_SEND_TAB_TO_SELF_PAGE_CONTEXT, pageContextBytes);
        }
        return intent;
    }

    private static Intent createTapIntent(String guid, String url) {
        return createTapIntent(guid, url, null);
    }

    private static Intent createTapIntent(byte @Nullable [] pageContextBytes) {
        return createTapIntent(GUID, URL, pageContextBytes);
    }

    private static Intent createTapIntent() {
        return createTapIntent(GUID, URL, null);
    }

    @Test
    @SmallTest
    public void testNotificationTap() {
        Intent intent = createTapIntent();

        // Add active notification.
        NotificationSharedPrefManager.addActiveNotification(
                new NotificationSharedPrefManager.ActiveNotification(1, GUID));

        NotificationManager.handleIntent(intent);

        // Verify that it marked it as opened and activated.
        verify(mBridgeMock).markEntryOpened(any(), eq(GUID));
        verify(mBridgeMock)
                .markEntryActivated(
                        any(), eq(GUID), eq(ShareActivatedEntryPoint.MOBILE_NOTIFICATION));
        verify(mMetricsMock).recordNotificationOpened();

        // Verify that the started intent is explicit and targets Chrome.
        Intent startedIntent =
                Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity();
        Assert.assertNotNull(startedIntent);
        Assert.assertEquals(Intent.ACTION_VIEW, startedIntent.getAction());
        Assert.assertEquals(URL, startedIntent.getDataString());
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
        Intent intent = createTapIntent(guid, url);

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
        Intent intent = createTapIntent(guid, url);

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

    /**
     * Verifies that tapping a notification with page context attaches the serialized page context
     * extra to the launcher intent when form field propagation is enabled.
     */
    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_PROPAGATE_FORM_FIELDS)
    public void testNotificationTap_WithPageContext_FeatureEnabled() {
        byte[] pageContextBytes = new byte[] {1, 2, 3, 4};
        Intent intent = createTapIntent(pageContextBytes);

        NotificationSharedPrefManager.addActiveNotification(
                new NotificationSharedPrefManager.ActiveNotification(1, GUID));

        NotificationManager.handleIntent(intent);

        // Verify that the started intent contains the serialized page context extra.
        Intent startedIntent =
                Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity();
        Assert.assertNotNull(startedIntent);
        Assert.assertArrayEquals(
                pageContextBytes,
                startedIntent.getByteArrayExtra(IntentHandler.EXTRA_SEND_TAB_TO_SELF_PAGE_CONTEXT));
    }

    /**
     * Verifies that tapping a notification with page context does not attach the page context extra
     * when form field propagation is disabled.
     */
    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_PROPAGATE_FORM_FIELDS)
    public void testNotificationTap_WithPageContext_FeatureDisabled() {
        byte[] pageContextBytes = new byte[] {1, 2, 3, 4};
        Intent intent = createTapIntent(pageContextBytes);

        NotificationSharedPrefManager.addActiveNotification(
                new NotificationSharedPrefManager.ActiveNotification(1, GUID));

        NotificationManager.handleIntent(intent);

        // Verify that the started intent does not contain the page context extra.
        Intent startedIntent =
                Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity();
        Assert.assertNotNull(startedIntent);
        Assert.assertNull(
                startedIntent.getByteArrayExtra(IntentHandler.EXTRA_SEND_TAB_TO_SELF_PAGE_CONTEXT));
    }

    /**
     * Verifies that tapping a notification without page context does not attach the page context
     * extra.
     */
    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_PROPAGATE_FORM_FIELDS)
    public void testNotificationTap_WithoutPageContext() {
        Intent intent = createTapIntent();

        NotificationSharedPrefManager.addActiveNotification(
                new NotificationSharedPrefManager.ActiveNotification(1, GUID));

        NotificationManager.handleIntent(intent);

        // Verify that no page context extra is present on the started intent.
        Intent startedIntent =
                Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity();
        Assert.assertNotNull(startedIntent);
        Assert.assertNull(
                startedIntent.getByteArrayExtra(IntentHandler.EXTRA_SEND_TAB_TO_SELF_PAGE_CONTEXT));
    }

    private static Intent getRealIntent(PendingIntent contentIntent) {
        Intent trampolineIntent = Shadows.shadowOf(contentIntent).getSavedIntent();
        PendingIntent pendingIntent =
                NotificationIntentInterceptor.getPendingIntentForTesting(trampolineIntent);
        return Shadows.shadowOf(pendingIntent).getSavedIntent();
    }

    /**
     * Verifies that NotificationManager.showNotification creates a notification whose content
     * intent carries the serialized page context extra when form field propagation is enabled.
     */
    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_PROPAGATE_FORM_FIELDS)
    public void testShowNotificationWithPageContext() {
        byte[] pageContextBytes = new byte[] {1, 2, 3};
        boolean shown =
                NotificationManager.showNotification(
                        GUID,
                        URL,
                        "title",
                        "device",
                        100000000L,
                        BroadcastReceiver.class,
                        null,
                        pageContextBytes);
        Assert.assertTrue(shown);

        ArgumentCaptor<NotificationWrapper> captor =
                ArgumentCaptor.forClass(NotificationWrapper.class);
        verify(mNotificationManagerProxy).notify(captor.capture());

        Intent tapIntent = getRealIntent(captor.getValue().getNotification().contentIntent);
        Assert.assertNotNull(tapIntent);
        Assert.assertEquals(
                NotificationManager.NOTIFICATION_ACTION_TAP, tapIntent.getAction());
        Assert.assertEquals(
                GUID, tapIntent.getStringExtra(NotificationManager.NOTIFICATION_GUID_EXTRA));
        Assert.assertArrayEquals(
                pageContextBytes,
                tapIntent.getByteArrayExtra(IntentHandler.EXTRA_SEND_TAB_TO_SELF_PAGE_CONTEXT));
    }

    /**
     * Verifies that NotificationManager.showNotification creates a notification without the page
     * context extra when no page context is provided.
     */
    @Test
    @SmallTest
    public void testShowNotificationWithoutPageContext() {
        boolean shown =
                NotificationManager.showNotification(
                        GUID,
                        URL,
                        "title",
                        "device",
                        100000000L,
                        BroadcastReceiver.class,
                        null,
                        null);
        Assert.assertTrue(shown);

        ArgumentCaptor<NotificationWrapper> captor =
                ArgumentCaptor.forClass(NotificationWrapper.class);
        verify(mNotificationManagerProxy).notify(captor.capture());

        Intent tapIntent = getRealIntent(captor.getValue().getNotification().contentIntent);
        Assert.assertNotNull(tapIntent);
        Assert.assertNull(
                tapIntent.getByteArrayExtra(IntentHandler.EXTRA_SEND_TAB_TO_SELF_PAGE_CONTEXT));
    }
}
