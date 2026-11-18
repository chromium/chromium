// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.app.AlarmManager;
import android.app.Notification;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.net.Uri;

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
import org.robolectric.shadows.ShadowAlarmManager;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationIntentInterceptor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.chrome.browser.profiles.ProfileTokenUtils;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationFeatureMap;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;

import java.nio.charset.StandardCharsets;
import java.util.concurrent.atomic.AtomicReference;

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
    @Mock private Profile mDeletedProfile;
    @Mock private ProfileResolver.Natives mProfileResolverNatives;
    private static final String GUID = "test_guid";
    private static final String URL = "https://www.example.com";
    private static final byte[] PROFILE_TOKEN = "profile_token".getBytes(StandardCharsets.UTF_8);
    private static final byte[] DELETED_PROFILE_TOKEN =
            "deleted_profile_token".getBytes(StandardCharsets.UTF_8);

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.getApplication());
        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mBridgeMock);
        SendTabToSelfMetricsRecorderJni.setInstanceForTesting(mMetricsMock);
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mNotificationManagerProxy);
        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);
        doReturn(PROFILE_TOKEN).when(mProfileResolverNatives).tokenizeProfileToBytes(mProfile);
        doAnswer(
                        invocation -> {
                            Callback<Profile> callback = invocation.getArgument(1);
                            callback.onResult(mProfile);
                            return null;
                        })
                .when(mProfileResolverNatives)
                .resolveProfileFromBytes(aryEq(PROFILE_TOKEN), any());
    }

    @After
    public void tearDown() {
        BaseNotificationManagerProxyFactory.setInstanceForTesting(null);
    }

    private Intent createTapIntent(String guid, String url, byte @Nullable [] pageContextBytes) {
        Intent intent =
                new Intent(ContextUtils.getApplicationContext(), BroadcastReceiver.class)
                        .setAction(NotificationManager.NOTIFICATION_ACTION_TAP)
                        .putExtra(NotificationManager.NOTIFICATION_GUID_EXTRA, guid)
                        .setData(Uri.parse(url));
        ProfileTokenUtils.addProfileToIntent(mProfile, intent);
        if (pageContextBytes != null) {
            intent.putExtra(IntentHandler.EXTRA_SEND_TAB_TO_SELF_PAGE_CONTEXT, pageContextBytes);
        }
        return intent;
    }

    private Intent createTapIntent(String guid, String url) {
        return createTapIntent(guid, url, null);
    }

    private Intent createTapIntent(byte @Nullable [] pageContextBytes) {
        return createTapIntent(GUID, URL, pageContextBytes);
    }

    private Intent createTapIntent() {
        return createTapIntent(GUID, URL, null);
    }

    /** Installs com.example.app with an activity that handles https://www.example.com/app links. */
    private static void addSpecializedHandler(@Nullable String appLabel) {
        ShadowPackageManager shadowPackageManager =
                Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager());

        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = "com.example.app";
        activityInfo.name = "com.example.app.MainActivity";
        activityInfo.nonLocalizedLabel = appLabel;
        shadowPackageManager.addOrUpdateActivity(activityInfo);

        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addDataScheme("https");
        filter.addDataAuthority("www.example.com", null);
        filter.addDataPath("/app", android.os.PatternMatcher.PATTERN_PREFIX);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        // NotificationManager resolves the activity with MATCH_DEFAULT_ONLY.
        filter.addCategory(Intent.CATEGORY_DEFAULT);
        shadowPackageManager.addIntentFilterForActivity(
                new ComponentName(activityInfo.packageName, activityInfo.name), filter);
    }

    @Test
    public void testNotificationTap() {
        Intent intent = createTapIntent();

        // Add active notification.
        NotificationSharedPrefManager.addActiveNotification(
                new NotificationSharedPrefManager.ActiveNotification(1, GUID));

        NotificationManager.handleIntent(intent);

        // Verify that it marked it as opened and activated.
        verify(mBridgeMock).markEntryOpened(eq(mProfile), eq(GUID));
        verify(mBridgeMock)
                .markEntryActivated(
                        eq(mProfile), eq(GUID), eq(ShareActivatedEntryPoint.MOBILE_NOTIFICATION));
        verify(mMetricsMock).recordNotificationStatus(NotificationStatus.OPENED);
        verify(mMetricsMock, never()).recordOpenedInNativeApp(anyInt());

        // Verify that the started intent is explicit and targets Chrome.
        Intent startedIntent =
                Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity();
        Assert.assertNotNull(startedIntent);
        Assert.assertEquals(Intent.ACTION_VIEW, startedIntent.getAction());
        Assert.assertEquals(URL, startedIntent.getDataString());
        Assert.assertEquals(
                "org.chromium.chrome.browser.document.ChromeLauncherActivity",
                startedIntent.getComponent().getClassName());
        // The launcher intent carries the notification's profile.
        Assert.assertTrue(ProfileTokenUtils.hasProfileToken(startedIntent));
    }

    /**
     * Verifies that a notification posted before profile tokens were attached to its intents falls
     * back to the last used regular profile.
     */
    @Test
    public void testNotificationTap_NoProfileToken() {
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        Intent intent =
                new Intent(ContextUtils.getApplicationContext(), BroadcastReceiver.class)
                        .setAction(NotificationManager.NOTIFICATION_ACTION_TAP)
                        .putExtra(NotificationManager.NOTIFICATION_GUID_EXTRA, GUID)
                        .setData(Uri.parse(URL));
        Assert.assertFalse(ProfileTokenUtils.hasProfileToken(intent));
        NotificationSharedPrefManager.addActiveNotification(
                new NotificationSharedPrefManager.ActiveNotification(1, GUID));

        NotificationManager.handleIntent(intent);

        verify(mProfileResolverNatives, never()).resolveProfileFromBytes(any(), any());
        verify(mProfileResolverNatives, never()).resolveProfile(any(), any());
        verify(mNotificationManagerProxy)
                .cancel(eq(NotificationConstants.GROUP_SEND_TAB_TO_SELF), anyInt());
        verify(mBridgeMock).markEntryOpened(eq(mProfile), eq(GUID));
        verify(mBridgeMock)
                .markEntryActivated(
                        eq(mProfile), eq(GUID), eq(ShareActivatedEntryPoint.MOBILE_NOTIFICATION));
        verify(mMetricsMock).recordNotificationStatus(NotificationStatus.OPENED);
        Intent startedIntent =
                Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity();
        Assert.assertNotNull(startedIntent);
        Assert.assertEquals(URL, startedIntent.getDataString());
        // The launcher intent carries the fallback profile.
        Assert.assertTrue(ProfileTokenUtils.hasProfileToken(startedIntent));
        verify(mProfileResolverNatives).tokenizeProfileToBytes(mProfile);
    }

    private void stubDeletedProfileResolution() {
        doReturn(DELETED_PROFILE_TOKEN)
                .when(mProfileResolverNatives)
                .tokenizeProfileToBytes(mDeletedProfile);
        doAnswer(
                        invocation -> {
                            Callback<@Nullable Profile> callback = invocation.getArgument(1);
                            callback.onResult(null);
                            return null;
                        })
                .when(mProfileResolverNatives)
                .resolveProfileFromBytes(aryEq(DELETED_PROFILE_TOKEN), any());
    }

    private static void assertIntentCarriesProfile(Intent intent, Profile expectedProfile) {
        Assert.assertTrue(ProfileTokenUtils.hasProfileToken(intent));
        AtomicReference<@Nullable Profile> resolvedProfile = new AtomicReference<>();
        ProfileTokenUtils.retrieveProfileFromIntent(intent, resolvedProfile::set);
        Assert.assertSame(expectedProfile, resolvedProfile.get());
    }

    private void verifyActionWithDeletedProfile(Intent intent) {
        stubDeletedProfileResolution();
        ProfileTokenUtils.addProfileToIntent(mDeletedProfile, intent);
        NotificationSharedPrefManager.addActiveNotification(
                new NotificationSharedPrefManager.ActiveNotification(1, GUID));

        NotificationManager.handleIntent(intent);

        verify(mProfileResolverNatives)
                .resolveProfileFromBytes(aryEq(DELETED_PROFILE_TOKEN), any());
        verify(mNotificationManagerProxy).cancel(NotificationConstants.GROUP_SEND_TAB_TO_SELF, 1);
        Assert.assertNull(NotificationSharedPrefManager.findActiveNotification(GUID));
        verifyNoInteractions(mBridgeMock);
        verify(mMetricsMock, never()).recordNotificationStatus(anyInt());
        Assert.assertNull(
                Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity());
    }

    /**
     * Verifies that if the profile token no longer resolves to a profile on tap (e.g. the profile
     * was deleted), only the notification is hidden without opening the URL, calling native bridge
     * methods, or recording status metrics.
     */
    @Test
    public void testNotificationTap_ProfileTokenResolvesToNull() {
        Intent intent =
                new Intent(ContextUtils.getApplicationContext(), BroadcastReceiver.class)
                        .setAction(NotificationManager.NOTIFICATION_ACTION_TAP)
                        .putExtra(NotificationManager.NOTIFICATION_GUID_EXTRA, GUID)
                        .setData(Uri.parse(URL));
        verifyActionWithDeletedProfile(intent);
        verify(mMetricsMock, never()).recordOpenedInNativeApp(anyInt());
    }

    /**
     * Verifies that if the profile token no longer resolves to a profile on dismiss (e.g. the
     * profile was deleted), only the notification is hidden and removed from the active pref
     * without calling into native or recording status metrics.
     */
    @Test
    public void testNotificationDismiss_ProfileTokenResolvesToNull() {
        Intent intent =
                new Intent(ContextUtils.getApplicationContext(), BroadcastReceiver.class)
                        .setAction(NotificationManager.NOTIFICATION_ACTION_DISMISS)
                        .putExtra(NotificationManager.NOTIFICATION_GUID_EXTRA, GUID);
        verifyActionWithDeletedProfile(intent);
    }

    /**
     * Verifies that if the profile token no longer resolves to a profile on timeout (e.g. the
     * profile was deleted), only the notification is hidden and removed from the active pref
     * without calling into native or recording status metrics.
     */
    @Test
    public void testNotificationTimeout_ProfileTokenResolvesToNull() {
        Intent intent =
                new Intent(ContextUtils.getApplicationContext(), BroadcastReceiver.class)
                        .setAction(NotificationManager.NOTIFICATION_ACTION_TIMEOUT)
                        .putExtra(NotificationManager.NOTIFICATION_GUID_EXTRA, GUID);
        verifyActionWithDeletedProfile(intent);
    }

    /**
     * Verifies that handling a tap intent waits for asynchronous profile resolution before opening
     * the URL, hiding the notification, or updating the entry.
     */
    @Test
    public void testNotificationTap_DeferredProfileResolution() {
        AtomicReference<Callback<@Nullable Profile>> pendingCallback = new AtomicReference<>();
        doAnswer(
                        invocation -> {
                            pendingCallback.set(invocation.getArgument(1));
                            return null;
                        })
                .when(mProfileResolverNatives)
                .resolveProfileFromBytes(aryEq(PROFILE_TOKEN), any());
        Intent intent = createTapIntent();
        NotificationSharedPrefManager.addActiveNotification(
                new NotificationSharedPrefManager.ActiveNotification(1, GUID));

        NotificationManager.handleIntent(intent);

        Assert.assertNotNull(pendingCallback.get());
        Assert.assertNull(
                Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity());
        verify(mNotificationManagerProxy, never()).cancel(any(), anyInt());
        verify(mBridgeMock, never()).markEntryOpened(any(), any());
        Assert.assertNotNull(NotificationSharedPrefManager.findActiveNotification(GUID));

        pendingCallback.get().onResult(mProfile);

        verify(mNotificationManagerProxy).cancel(NotificationConstants.GROUP_SEND_TAB_TO_SELF, 1);
        Assert.assertNull(NotificationSharedPrefManager.findActiveNotification(GUID));
        verify(mBridgeMock).markEntryOpened(mProfile, GUID);
        verify(mBridgeMock)
                .markEntryActivated(mProfile, GUID, ShareActivatedEntryPoint.MOBILE_NOTIFICATION);
        verify(mMetricsMock).recordNotificationStatus(NotificationStatus.OPENED);
        Intent startedIntent =
                Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity();
        Assert.assertNotNull(startedIntent);
        Assert.assertEquals(URL, startedIntent.getDataString());
        Assert.assertTrue(ProfileTokenUtils.hasProfileToken(startedIntent));
        pendingCallback.set(null);
        AtomicReference<@Nullable Profile> startedProfile = new AtomicReference<>();
        ProfileTokenUtils.retrieveProfileFromIntent(startedIntent, startedProfile::set);
        Assert.assertNotNull(pendingCallback.get());
        pendingCallback.get().onResult(mProfile);
        Assert.assertSame(mProfile, startedProfile.get());
    }

    /**
     * Verifies that dismissing a notification without a profile token hides it and dismisses the
     * entry for the last used regular profile.
     */
    @Test
    public void testNotificationDismiss_NoProfileToken() {
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        Intent intent =
                new Intent(ContextUtils.getApplicationContext(), BroadcastReceiver.class)
                        .setAction(NotificationManager.NOTIFICATION_ACTION_DISMISS)
                        .putExtra(NotificationManager.NOTIFICATION_GUID_EXTRA, GUID);
        NotificationSharedPrefManager.addActiveNotification(
                new NotificationSharedPrefManager.ActiveNotification(1, GUID));

        NotificationManager.handleIntent(intent);

        verify(mNotificationManagerProxy)
                .cancel(eq(NotificationConstants.GROUP_SEND_TAB_TO_SELF), anyInt());
        verify(mBridgeMock).dismissEntry(eq(mProfile), eq(GUID));
        verify(mMetricsMock).recordNotificationStatus(NotificationStatus.DISMISSED);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_OPEN_NATIVE_APP)
    public void testNotificationTapWithSpecializedHandler() {
        String guid = "test_guid";
        String url = "https://www.example.com/app/path";
        Intent intent = createTapIntent(guid, url);

        // Add active notification.
        NotificationSharedPrefManager.addActiveNotification(
                new NotificationSharedPrefManager.ActiveNotification(1, guid));

        // Register a specialized handler.
        addSpecializedHandler(/* appLabel= */ null);

        NotificationManager.handleIntent(intent);

        // Verify that it marked it as opened and activated.
        verify(mBridgeMock).markEntryOpened(eq(mProfile), eq(guid));
        verify(mBridgeMock)
                .markEntryActivated(
                        eq(mProfile), eq(guid), eq(ShareActivatedEntryPoint.MOBILE_NOTIFICATION));
        verify(mMetricsMock).recordNotificationStatus(NotificationStatus.OPENED);
        verify(mMetricsMock).recordOpenedInNativeApp(NativeAppOpenEntryPoint.MOBILE_NOTIFICATION);

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
    public void testNotificationTapWithSpecializedHandlerButFeatureDisabled() {
        String guid = "test_guid";
        String url = "https://www.example.com/app/path";
        Intent intent = createTapIntent(guid, url);

        // Add active notification.
        NotificationSharedPrefManager.addActiveNotification(
                new NotificationSharedPrefManager.ActiveNotification(1, guid));

        // Register a specialized handler.
        addSpecializedHandler(/* appLabel= */ null);

        NotificationManager.handleIntent(intent);

        // Verify that it marked it as opened and activated.
        verify(mBridgeMock).markEntryOpened(eq(mProfile), eq(guid));
        verify(mBridgeMock)
                .markEntryActivated(
                        eq(mProfile), eq(guid), eq(ShareActivatedEntryPoint.MOBILE_NOTIFICATION));
        verify(mMetricsMock).recordNotificationStatus(NotificationStatus.OPENED);
        verify(mMetricsMock, never()).recordOpenedInNativeApp(anyInt());

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
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_OPEN_NATIVE_APP)
    public void testOpenInNativeAppIfPossibleWithSpecializedHandler() {
        String url = "https://www.example.com/app/path";
        Uri uri = Uri.parse(url);

        // Register a specialized handler.
        addSpecializedHandler(/* appLabel= */ null);

        Assert.assertNotNull(NotificationManager.getMatchingNativeAppResolveInfo(uri));
        Assert.assertTrue(NotificationManager.canOpenInNativeApp(uri));

        boolean result =
                NotificationManager.openInNativeAppIfPossible(
                        uri, NativeAppOpenEntryPoint.MOBILE_NOTIFICATION);
        Assert.assertTrue(result);
        verify(mMetricsMock).recordOpenedInNativeApp(NativeAppOpenEntryPoint.MOBILE_NOTIFICATION);

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
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_OPEN_NATIVE_APP)
    public void testOpenInNativeAppIfPossibleNoSpecializedHandler() {
        String url = "https://www.example.com/app/path";
        Uri uri = Uri.parse(url);
        // Do not register specialized handler.

        Assert.assertNull(NotificationManager.getMatchingNativeAppResolveInfo(uri));
        Assert.assertFalse(NotificationManager.canOpenInNativeApp(uri));

        boolean result =
                NotificationManager.openInNativeAppIfPossible(
                        uri, NativeAppOpenEntryPoint.MOBILE_NOTIFICATION);
        Assert.assertFalse(result);
        verify(mMetricsMock, never()).recordOpenedInNativeApp(anyInt());

        Intent startedIntent =
                Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity();
        Assert.assertNull(startedIntent);
    }

    /**
     * Verifies that `openInNativeAppIfPossible` returns `false` and does not record the native app
     * open metric when starting the activity fails (`ActivityNotFoundException`).
     */
    @Test
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_OPEN_NATIVE_APP)
    public void testOpenInNativeAppIfPossible_StartActivityFails_DoesNotRecordMetric() {
        String url = "https://www.example.com/app/path";
        Uri uri = Uri.parse(url);

        // Force `canOpenInNativeApp` to return `true` while no activity is actually registered in
        // `PackageManager`, and instruct Robolectric's `ShadowApplication` to throw
        // `ActivityNotFoundException` when an unresolvable intent is started.
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = new ActivityInfo();
        resolveInfo.activityInfo.packageName = "com.example.app";
        resolveInfo.activityInfo.name = "com.example.app.MissingActivity";
        NotificationManager.setResolveInfoForTesting(resolveInfo);
        Shadows.shadowOf(RuntimeEnvironment.getApplication()).checkActivities(true);

        boolean result =
                NotificationManager.openInNativeAppIfPossible(
                        uri, NativeAppOpenEntryPoint.MOBILE_NOTIFICATION);
        Assert.assertFalse(result);
        verify(mMetricsMock, never()).recordOpenedInNativeApp(anyInt());
    }

    /**
     * Verifies that tapping a notification with page context attaches the serialized page context
     * extra to the launcher intent.
     */
    @Test
    public void testNotificationTap_WithPageContext() {
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
     * Verifies that tapping a notification without page context does not attach the page context
     * extra.
     */
    @Test
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
     * Verifies that `NotificationManager.showNotification` creates a notification whose content
     * intent carries the serialized page context extra.
     */
    @Test
    public void testShowNotificationWithPageContext() {
        byte[] pageContextBytes = new byte[] {1, 2, 3};
        boolean shown =
                NotificationManager.showNotification(
                        mProfile,
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
        Assert.assertEquals(NotificationManager.NOTIFICATION_ACTION_TAP, tapIntent.getAction());
        Assert.assertEquals(
                GUID, tapIntent.getStringExtra(NotificationManager.NOTIFICATION_GUID_EXTRA));
        assertIntentCarriesProfile(tapIntent, mProfile);
        Assert.assertArrayEquals(
                pageContextBytes,
                tapIntent.getByteArrayExtra(IntentHandler.EXTRA_SEND_TAB_TO_SELF_PAGE_CONTEXT));

        Intent dismissIntent = getRealIntent(captor.getValue().getNotification().deleteIntent);
        Assert.assertNotNull(dismissIntent);
        Assert.assertEquals(
                NotificationManager.NOTIFICATION_ACTION_DISMISS, dismissIntent.getAction());
        Assert.assertEquals(
                GUID, dismissIntent.getStringExtra(NotificationManager.NOTIFICATION_GUID_EXTRA));
        assertIntentCarriesProfile(dismissIntent, mProfile);

        ShadowAlarmManager shadowAlarmManager =
                Shadows.shadowOf(
                        (AlarmManager)
                                ContextUtils.getApplicationContext()
                                        .getSystemService(Context.ALARM_SERVICE));
        ShadowAlarmManager.ScheduledAlarm scheduledAlarm =
                shadowAlarmManager.getNextScheduledAlarm();
        Assert.assertNotNull(scheduledAlarm);
        Intent timeoutIntent = Shadows.shadowOf(scheduledAlarm.operation).getSavedIntent();
        Assert.assertNotNull(timeoutIntent);
        Assert.assertEquals(
                NotificationManager.NOTIFICATION_ACTION_TIMEOUT, timeoutIntent.getAction());
        Assert.assertEquals(
                GUID, timeoutIntent.getStringExtra(NotificationManager.NOTIFICATION_GUID_EXTRA));
        assertIntentCarriesProfile(timeoutIntent, mProfile);
    }

    /**
     * Verifies that NotificationManager.showNotification creates a notification without the page
     * context extra when no page context is provided.
     */
    @Test
    public void testShowNotificationWithoutPageContext() {
        boolean shown =
                NotificationManager.showNotification(
                        mProfile,
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

    /**
     * Verifies that showing, dismissing, and timing out a notification correctly record the unified
     * NotificationStatus metric.
     */
    @Test
    public void testNotificationStatusMetricsOnShowDismissAndTimeout() {
        NotificationManager.showNotification(
                mProfile,
                GUID,
                URL,
                "title",
                "device",
                100000000L,
                BroadcastReceiver.class,
                null,
                null);
        verify(mMetricsMock).recordNotificationStatus(NotificationStatus.SHOWN);

        Intent dismissIntent =
                new Intent(ContextUtils.getApplicationContext(), BroadcastReceiver.class)
                        .setAction(NotificationManager.NOTIFICATION_ACTION_DISMISS)
                        .putExtra(NotificationManager.NOTIFICATION_GUID_EXTRA, GUID);
        ProfileTokenUtils.addProfileToIntent(mProfile, dismissIntent);
        NotificationManager.handleIntent(dismissIntent);
        verify(mNotificationManagerProxy)
                .cancel(eq(NotificationConstants.GROUP_SEND_TAB_TO_SELF), anyInt());
        verify(mMetricsMock).recordNotificationStatus(NotificationStatus.DISMISSED);

        Intent timeoutIntent =
                new Intent(ContextUtils.getApplicationContext(), BroadcastReceiver.class)
                        .setAction(NotificationManager.NOTIFICATION_ACTION_TIMEOUT)
                        .putExtra(NotificationManager.NOTIFICATION_GUID_EXTRA, GUID);
        ProfileTokenUtils.addProfileToIntent(mProfile, timeoutIntent);
        NotificationManager.handleIntent(timeoutIntent);
        verify(mMetricsMock).recordNotificationStatus(NotificationStatus.TIMED_OUT);
    }

    @Test
    public void testShowNotification_OpensInTab_ContextTextContainsHost() {
        boolean shown =
                NotificationManager.showNotification(
                        mProfile,
                        GUID,
                        URL,
                        "title",
                        "My Phone",
                        100000000L,
                        BroadcastReceiver.class,
                        null,
                        null);
        Assert.assertTrue(shown);

        ArgumentCaptor<NotificationWrapper> captor =
                ArgumentCaptor.forClass(NotificationWrapper.class);
        verify(mNotificationManagerProxy).notify(captor.capture());

        Notification notification = captor.getValue().getNotification();
        Assert.assertEquals(
                "www.example.com - Sent from My Phone",
                notification.extras.getString(Notification.EXTRA_TEXT));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_OPEN_NATIVE_APP)
    public void testShowNotification_OpensInNativeApp_ContextTextContainsAppName() {
        String url = "https://www.example.com/app/path";
        addSpecializedHandler("Example App");

        boolean shown =
                NotificationManager.showNotification(
                        mProfile,
                        GUID,
                        url,
                        "title",
                        "My Phone",
                        100000000L,
                        BroadcastReceiver.class,
                        null,
                        null);
        Assert.assertTrue(shown);

        ArgumentCaptor<NotificationWrapper> captor =
                ArgumentCaptor.forClass(NotificationWrapper.class);
        verify(mNotificationManagerProxy).notify(captor.capture());

        Notification notification = captor.getValue().getNotification();
        Assert.assertEquals(
                "Open in Example App - Sent from My Phone",
                notification.extras.getString(Notification.EXTRA_TEXT));
    }
}
