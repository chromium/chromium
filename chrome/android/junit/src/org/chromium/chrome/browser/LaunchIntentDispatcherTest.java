// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ComponentCaller;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Process;
import android.provider.Browser;

import androidx.browser.auth.AuthTabIntent;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.TrustedWebUtils;
import androidx.browser.trusted.FileHandlingData;
import androidx.browser.trusted.TrustedWebActivityIntentBuilder;
import androidx.browser.trusted.sharing.ShareData;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.actor.ActorForegroundServiceController;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorNotificationFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.browserservices.SessionDataHolder;
import org.chromium.chrome.browser.browserservices.SessionHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.glic.GlicIntentConstants;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelType;
import org.chromium.chrome.browser.tabwindow.TabWindowInfo;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.browser_ui.notifications.ForegroundServiceUtils;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.externalauth.ExternalAuthUtils;

import java.util.Arrays;

/** Unit tests for {@link LaunchIntentDispatcher}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.CCT_DONT_OVERRIDE_INTENT_MIME_TYPE,
    ChromeFeatureList.GLIC_BACKGROUND_TRIGGERING,
    ChromeFeatureList.GLIC_BACKGROUND_ACTUATION,
    ChromeFeatureList.ACTOR_NOTIFICATION_INTENT_ROUTING
})
public class LaunchIntentDispatcherTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private CustomTabsConnection mCustomTabsConnection;
    @Mock private SessionDataHolder mSessionDataHolder;
    @Mock private SessionHandler mSessionHandler;
    @Mock private ActivityManager mActivityManager;
    @Mock IntentHandler.Natives mIntentHandlerNativeMock;
    @Mock ExternalIntentUrlChecker.Natives mExternalIntentUrlCheckerNativeMock;
    @Mock private ExternalAuthUtils mExternalAuthUtils;
    @Mock private ForegroundServiceUtils mForegroundServiceUtils;
    @Mock private ChromeBrowserInitializer mChromeBrowserInitializer;
    @Mock private Profile mProfile;
    @Mock private GlicEnabling.Natives mGlicEnablingJniMock;

    private Activity mActivity;

    @Before
    public void setUp() {
        ExternalIntentUrlCheckerJni.setInstanceForTesting(mExternalIntentUrlCheckerNativeMock);
        doReturn(true).when(mExternalIntentUrlCheckerNativeMock).validateUrl(any());
        IntentHandlerJni.setInstanceForTesting(mIntentHandlerNativeMock);

        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtils);
        ForegroundServiceUtils.setInstanceForTesting(mForegroundServiceUtils);
        ChromeBrowserInitializer.setForTesting(mChromeBrowserInitializer);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        GlicEnablingJni.setInstanceForTesting(mGlicEnablingJniMock);

        mActivity = Robolectric.buildActivity(Activity.class).get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        CustomTabsConnection.setInstanceForTesting(mCustomTabsConnection);
        SessionDataHolder.setInstanceForTesting(mSessionDataHolder);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.CUSTOM_TABS_ALWAYS_OPEN_IN_BROWSER);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.CUSTOM_TABS_ALWAYS_OPEN_IN_BROWSER);
        ActorKeyedServiceFactory.setForTesting(null);
        ActorForegroundServiceController.setInstanceForTesting(null);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(null);
        MultiWindowTestUtils.resetInstanceInfo();
    }

    @Test
    public void testDispatchToCustomTabActivity_DelegatesToExistingHandler() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);

        doReturn(mSessionHandler).when(mSessionDataHolder).getActiveHandlerForIntent(any());
        doReturn(true).when(mSessionHandler).handleIntent(any());
        int taskId = 123;
        doReturn(taskId).when(mSessionHandler).getTaskId();

        Activity spyActivity = spy(mActivity);
        doReturn(mActivityManager).when(spyActivity).getSystemService(Context.ACTIVITY_SERVICE);

        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(mSessionHandler).handleIntent(intent);
        verify(mActivityManager).moveTaskToFront(eq(taskId), anyInt());
    }

    @Test
    public void testDispatchToCustomTabActivity_StartsNewActivityIfNoHandler() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);

        doReturn(null).when(mSessionDataHolder).getActiveHandlerForIntent(any());

        Activity spyActivity = spy(mActivity);

        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(spyActivity).startActivity(any(), any());
        verifyNoInteractions(mSessionHandler);
    }

    @Test
    public void testDispatchToCustomTabActivity_CrossTaskTwaRouting_DelegatesToExistingHandler() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);
        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true);
        intent.putExtra(IntentHandler.EXTRA_CCT_EARLY_NAV, true);
        intent.putExtra(IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE, "com.spoofed.app");

        doReturn(null).when(mSessionDataHolder).getActiveHandlerClassInCurrentTask(any(), any());
        doReturn(mSessionHandler).when(mSessionDataHolder).getHandlerForIntent(any());
        doReturn(true).when(mSessionHandler).handleIntent(any());
        int taskId = 456;
        doReturn(taskId).when(mSessionHandler).getTaskId();

        Activity spyActivity = spy(mActivity);
        doReturn(mActivityManager).when(spyActivity).getSystemService(Context.ACTIVITY_SERVICE);
        Uri referrer = Uri.parse("android-app://com.example.referrer");
        doReturn(referrer).when(spyActivity).getReferrer();
        doReturn("com.example.caller").when(spyActivity).getCallingPackage();

        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(mActivityManager).moveTaskToFront(eq(taskId), anyInt());
        verify(spyActivity, never()).startActivity(any(), any());

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mSessionHandler).handleIntent(intentCaptor.capture());
        Intent forwardedIntent = intentCaptor.getValue();
        assertFalse(forwardedIntent.hasExtra(IntentHandler.EXTRA_CCT_EARLY_NAV));
        assertEquals(
                referrer.toString(),
                forwardedIntent.getStringExtra(IntentHandler.EXTRA_ACTIVITY_REFERRER));
        assertEquals(
                "com.example.caller",
                forwardedIntent.getStringExtra(IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE));
        assertTrue(
                forwardedIntent.getBooleanExtra(
                        TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false));
    }

    @Test
    public void testDispatchToCustomTabActivity_CrossTaskTwaRouting_StartsNewActivityIfNoHandler() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);
        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true);

        doReturn(null).when(mSessionDataHolder).getActiveHandlerClassInCurrentTask(any(), any());
        doReturn(null).when(mSessionDataHolder).getHandlerForIntent(any());

        Activity spyActivity = spy(mActivity);

        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(spyActivity).startActivity(any(), any());
        verifyNoInteractions(mSessionHandler);
    }

    @Test
    public void
            testDispatchToCustomTabActivity_CrossTaskTwaRouting_StartsNewActivityIfHandlerReturnsFalse() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);
        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true);

        doReturn(null).when(mSessionDataHolder).getActiveHandlerClassInCurrentTask(any(), any());
        doReturn(mSessionHandler).when(mSessionDataHolder).getHandlerForIntent(any());
        doReturn(false).when(mSessionHandler).handleIntent(any());

        Activity spyActivity = spy(mActivity);
        doReturn(mActivityManager).when(spyActivity).getSystemService(Context.ACTIVITY_SERVICE);

        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(mSessionHandler).handleIntent(any());
        verify(mActivityManager, never()).moveTaskToFront(anyInt(), anyInt());
        verify(spyActivity).startActivity(any(), any());
    }

    @Test
    public void testDispatchToCustomTabActivity_NonTwa_BypassesCrossTaskRouting() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);

        doReturn(null).when(mSessionDataHolder).getActiveHandlerForIntent(any());

        Activity spyActivity = spy(mActivity);

        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(mSessionDataHolder, never()).getActiveHandlerClassInCurrentTask(any(), any());
        verify(mSessionDataHolder, never()).getHandlerForIntent(any());
        verify(spyActivity).startActivity(any(), any());
        verifyNoInteractions(mSessionHandler);
    }

    @Test
    public void testDispatchToCustomTabActivity_TwaHandlerInCurrentTask_UsesClearTopLaunch() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);
        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true);

        doReturn(CustomTabActivity.class)
                .when(mSessionDataHolder)
                .getActiveHandlerClassInCurrentTask(any(), any());

        Activity spyActivity = spy(mActivity);

        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(mSessionDataHolder, never()).getHandlerForIntent(any());
        verifyNoInteractions(mSessionHandler);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(intentCaptor.capture(), any());
        Intent launchedIntent = intentCaptor.getValue();
        assertEquals(
                CustomTabActivity.class.getName(), launchedIntent.getComponent().getClassName());
        assertTrue((launchedIntent.getFlags() & Intent.FLAG_ACTIVITY_CLEAR_TOP) != 0);
        assertTrue((launchedIntent.getFlags() & Intent.FLAG_ACTIVITY_SINGLE_TOP) != 0);
    }

    private static final int TEST_UID = 12345;
    private static final int TEST_PID = 6789;

    private void mockSessionUid() {
        doReturn(TEST_UID).when(mCustomTabsConnection).getClientUidForSession(any());
        doReturn(TEST_PID).when(mCustomTabsConnection).getClientPidForSession(any());
    }

    @Test
    public void testFileHandling_StashesDataIfPermissionGranted() {
        Uri fileUri = Uri.parse("content://com.example/file.txt");
        FileHandlingData fileHandlingData = new FileHandlingData(Arrays.asList(fileUri));
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        IBinder sessionBinder = mock(IBinder.class);
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, sessionBinder);
        intent.putExtra(
                TrustedWebActivityIntentBuilder.EXTRA_FILE_HANDLING_DATA,
                fileHandlingData.toBundle());

        doReturn(null).when(mSessionDataHolder).getActiveHandlerForIntent(any());

        Activity spyActivity = spy(mActivity);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            ComponentCaller mockCaller = mock(ComponentCaller.class);
            doReturn(Process.myUid() + 1).when(mockCaller).getUid();
            doReturn(PackageManager.PERMISSION_GRANTED)
                    .when(mockCaller)
                    .checkContentUriPermission(
                            eq(fileUri), eq(Intent.FLAG_GRANT_READ_URI_PERMISSION));
            doReturn(PackageManager.PERMISSION_GRANTED)
                    .when(mockCaller)
                    .checkContentUriPermission(
                            eq(fileUri), eq(Intent.FLAG_GRANT_WRITE_URI_PERMISSION));
            doReturn(mockCaller).when(spyActivity).getInitialCaller();
        } else {
            mockSessionUid();
            // Grant read and write permissions
            doReturn(PackageManager.PERMISSION_GRANTED)
                    .when(spyActivity)
                    .checkUriPermission(
                            eq(fileUri),
                            eq(TEST_PID),
                            eq(TEST_UID),
                            eq(Intent.FLAG_GRANT_READ_URI_PERMISSION));
            doReturn(PackageManager.PERMISSION_GRANTED)
                    .when(spyActivity)
                    .checkUriPermission(
                            eq(fileUri),
                            eq(TEST_PID),
                            eq(TEST_UID),
                            eq(Intent.FLAG_GRANT_WRITE_URI_PERMISSION));
        }

        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(intentCaptor.capture(), any());
        Intent launchedIntent = intentCaptor.getValue();

        // Verify stashed data
        Bundle stashedBundle =
                launchedIntent.getBundleExtra(
                        CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_HANDLING_DATA);
        FileHandlingData stashedData = FileHandlingData.fromBundle(stashedBundle);
        assertEquals(1, stashedData.uris.size());
        assertEquals(fileUri, stashedData.uris.get(0));

        boolean[] canWrite =
                launchedIntent.getBooleanArrayExtra(
                        CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_CAN_WRITE);
        assertEquals(1, canWrite.length);
        assertEquals(true, canWrite[0]);
    }

    @Test
    public void testFileHandling_StashesReadOnlyIfWriteDenied() {
        Uri fileUri = Uri.parse("content://com.example/file.txt");
        FileHandlingData fileHandlingData = new FileHandlingData(Arrays.asList(fileUri));
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        IBinder sessionBinder = mock(IBinder.class);
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, sessionBinder);
        intent.putExtra(
                TrustedWebActivityIntentBuilder.EXTRA_FILE_HANDLING_DATA,
                fileHandlingData.toBundle());

        doReturn(null).when(mSessionDataHolder).getActiveHandlerForIntent(any());

        Activity spyActivity = spy(mActivity);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            ComponentCaller mockCaller = mock(ComponentCaller.class);
            doReturn(Process.myUid() + 1).when(mockCaller).getUid();
            doReturn(PackageManager.PERMISSION_GRANTED)
                    .when(mockCaller)
                    .checkContentUriPermission(
                            eq(fileUri), eq(Intent.FLAG_GRANT_READ_URI_PERMISSION));
            doReturn(PackageManager.PERMISSION_DENIED)
                    .when(mockCaller)
                    .checkContentUriPermission(
                            eq(fileUri), eq(Intent.FLAG_GRANT_WRITE_URI_PERMISSION));
            doReturn(mockCaller).when(spyActivity).getInitialCaller();
        } else {
            mockSessionUid();
            // Grant read but deny write
            doReturn(PackageManager.PERMISSION_GRANTED)
                    .when(spyActivity)
                    .checkUriPermission(
                            eq(fileUri),
                            eq(TEST_PID),
                            eq(TEST_UID),
                            eq(Intent.FLAG_GRANT_READ_URI_PERMISSION));
            doReturn(PackageManager.PERMISSION_DENIED)
                    .when(spyActivity)
                    .checkUriPermission(
                            eq(fileUri),
                            eq(TEST_PID),
                            eq(TEST_UID),
                            eq(Intent.FLAG_GRANT_WRITE_URI_PERMISSION));
        }

        LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(intentCaptor.capture(), any());
        Intent launchedIntent = intentCaptor.getValue();

        Bundle stashedBundle =
                launchedIntent.getBundleExtra(
                        CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_HANDLING_DATA);
        FileHandlingData stashedData = FileHandlingData.fromBundle(stashedBundle);
        assertEquals(1, stashedData.uris.size());
        assertEquals(fileUri, stashedData.uris.get(0));

        boolean[] canWrite =
                launchedIntent.getBooleanArrayExtra(
                        CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_CAN_WRITE);
        assertEquals(1, canWrite.length);
        assertEquals(false, canWrite[0]);
    }

    @Test
    public void testFileHandling_StashesEmptyIfReadDenied() {
        Uri fileUri = Uri.parse("content://com.example/file.txt");
        FileHandlingData fileHandlingData = new FileHandlingData(Arrays.asList(fileUri));
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        IBinder sessionBinder = mock(IBinder.class);
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, sessionBinder);
        intent.putExtra(
                TrustedWebActivityIntentBuilder.EXTRA_FILE_HANDLING_DATA,
                fileHandlingData.toBundle());

        doReturn(null).when(mSessionDataHolder).getActiveHandlerForIntent(any());

        Activity spyActivity = spy(mActivity);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            ComponentCaller mockCaller = mock(ComponentCaller.class);
            doReturn(Process.myUid() + 1).when(mockCaller).getUid();
            doReturn(PackageManager.PERMISSION_DENIED)
                    .when(mockCaller)
                    .checkContentUriPermission(
                            eq(fileUri), eq(Intent.FLAG_GRANT_READ_URI_PERMISSION));
            doReturn(mockCaller).when(spyActivity).getInitialCaller();
        } else {
            mockSessionUid();
            // Deny read
            doReturn(PackageManager.PERMISSION_DENIED)
                    .when(spyActivity)
                    .checkUriPermission(
                            eq(fileUri),
                            eq(TEST_PID),
                            eq(TEST_UID),
                            eq(Intent.FLAG_GRANT_READ_URI_PERMISSION));
        }

        LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(intentCaptor.capture(), any());
        Intent launchedIntent = intentCaptor.getValue();

        Bundle stashedBundle =
                launchedIntent.getBundleExtra(
                        CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_HANDLING_DATA);
        FileHandlingData stashedData = FileHandlingData.fromBundle(stashedBundle);
        assertEquals(0, stashedData.uris.size());

        boolean[] canWrite =
                launchedIntent.getBooleanArrayExtra(
                        CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_CAN_WRITE);
        org.junit.Assert.assertNull(canWrite);
    }

    @Test
    public void testFileHandling_NoExtrasIfNoData() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);

        doReturn(null).when(mSessionDataHolder).getActiveHandlerForIntent(any());

        Activity spyActivity = spy(mActivity);

        LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(intentCaptor.capture(), any());
        Intent launchedIntent = intentCaptor.getValue();

        assertEquals(
                false,
                launchedIntent.hasExtra(
                        CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_HANDLING_DATA));
        assertEquals(
                false,
                launchedIntent.hasExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_CAN_WRITE));
    }

    @Test
    public void testFileHandling_StripsSpoofedVerifiedData() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);
        FileHandlingData spoofedData =
                new FileHandlingData(Arrays.asList(Uri.parse("content://spoofed/data")));
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_HANDLING_DATA,
                spoofedData.toBundle());
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_CAN_WRITE, new boolean[] {true});

        doReturn(null).when(mSessionDataHolder).getActiveHandlerForIntent(any());

        Activity spyActivity = spy(mActivity);

        LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(intentCaptor.capture(), any());
        Intent launchedIntent = intentCaptor.getValue();

        assertEquals(
                false,
                launchedIntent.hasExtra(
                        CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_HANDLING_DATA));
        assertEquals(
                false,
                launchedIntent.hasExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_CAN_WRITE));
    }

    @Test
    public void testFileHandling_DelegatedToExistingHandler_StripsSpoofedVerifiedData() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);
        FileHandlingData clientData =
                new FileHandlingData(Arrays.asList(Uri.parse("content://client/data")));
        intent.putExtra(
                TrustedWebActivityIntentBuilder.EXTRA_FILE_HANDLING_DATA, clientData.toBundle());
        FileHandlingData spoofedData =
                new FileHandlingData(Arrays.asList(Uri.parse("content://spoofed/data")));
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_HANDLING_DATA,
                spoofedData.toBundle());
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_CAN_WRITE, new boolean[] {true});

        doReturn(mSessionHandler).when(mSessionDataHolder).getActiveHandlerForIntent(any());
        doReturn(true).when(mSessionHandler).handleIntent(any());
        doReturn(123).when(mSessionHandler).getTaskId();

        Activity spyActivity = spy(mActivity);
        doReturn(mActivityManager).when(spyActivity).getSystemService(Context.ACTIVITY_SERVICE);

        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mSessionHandler).handleIntent(intentCaptor.capture());
        Intent deliveredIntent = intentCaptor.getValue();

        assertFalse(
                deliveredIntent.hasExtra(
                        CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_HANDLING_DATA));
        assertFalse(
                deliveredIntent.hasExtra(
                        CustomTabIntentDataProvider.EXTRA_VERIFIED_FILE_CAN_WRITE));
        assertTrue(
                deliveredIntent.hasExtra(TrustedWebActivityIntentBuilder.EXTRA_FILE_HANDLING_DATA));
    }

    @Test
    public void testShareTarget_CallerHasReadPermission_StashesVerifiedShareData() {
        Uri fileUri = Uri.parse("content://com.example/shared_file.jpg");
        ShareData shareData = new ShareData("share_title", "share_text", Arrays.asList(fileUri));
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        IBinder sessionBinder = mock(IBinder.class);
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, sessionBinder);
        intent.putExtra(TrustedWebActivityIntentBuilder.EXTRA_SHARE_DATA, shareData.toBundle());

        doReturn(null).when(mSessionDataHolder).getActiveHandlerForIntent(any());

        Activity spyActivity = spy(mActivity);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            ComponentCaller mockCaller = mock(ComponentCaller.class);
            doReturn(Process.myUid() + 1).when(mockCaller).getUid();
            doReturn(PackageManager.PERMISSION_GRANTED)
                    .when(mockCaller)
                    .checkContentUriPermission(
                            eq(fileUri), eq(Intent.FLAG_GRANT_READ_URI_PERMISSION));
            doReturn(mockCaller).when(spyActivity).getInitialCaller();
        } else {
            mockSessionUid();
            doReturn(PackageManager.PERMISSION_GRANTED)
                    .when(spyActivity)
                    .checkUriPermission(
                            eq(fileUri),
                            eq(TEST_PID),
                            eq(TEST_UID),
                            eq(Intent.FLAG_GRANT_READ_URI_PERMISSION));
        }

        LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(intentCaptor.capture(), any());
        Intent launchedIntent = intentCaptor.getValue();

        Bundle stashedBundle =
                launchedIntent.getBundleExtra(
                        CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        ShareData stashedData = ShareData.fromBundle(stashedBundle);
        assertEquals("share_title", stashedData.title);
        assertEquals("share_text", stashedData.text);
        assertEquals(1, stashedData.uris.size());
        assertEquals(fileUri, stashedData.uris.get(0));
    }

    @Test
    public void testShareTarget_CallerDeniedReadPermission_StashesEmptyUris() {
        Uri fileUri = Uri.parse("content://com.example/secret_file.pdf");
        ShareData shareData = new ShareData("share_title", "share_text", Arrays.asList(fileUri));
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        IBinder sessionBinder = mock(IBinder.class);
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, sessionBinder);
        intent.putExtra(TrustedWebActivityIntentBuilder.EXTRA_SHARE_DATA, shareData.toBundle());

        doReturn(null).when(mSessionDataHolder).getActiveHandlerForIntent(any());

        Activity spyActivity = spy(mActivity);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            ComponentCaller mockCaller = mock(ComponentCaller.class);
            doReturn(Process.myUid() + 1).when(mockCaller).getUid();
            doReturn(PackageManager.PERMISSION_DENIED)
                    .when(mockCaller)
                    .checkContentUriPermission(
                            eq(fileUri), eq(Intent.FLAG_GRANT_READ_URI_PERMISSION));
            doReturn(mockCaller).when(spyActivity).getInitialCaller();
        } else {
            mockSessionUid();
            doReturn(PackageManager.PERMISSION_DENIED)
                    .when(spyActivity)
                    .checkUriPermission(
                            eq(fileUri),
                            eq(TEST_PID),
                            eq(TEST_UID),
                            eq(Intent.FLAG_GRANT_READ_URI_PERMISSION));
        }

        LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(intentCaptor.capture(), any());
        Intent launchedIntent = intentCaptor.getValue();

        Bundle stashedBundle =
                launchedIntent.getBundleExtra(
                        CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        ShareData stashedData = ShareData.fromBundle(stashedBundle);
        assertEquals("share_title", stashedData.title);
        assertEquals("share_text", stashedData.text);
        assertEquals(0, stashedData.uris.size());
    }

    @Test
    public void testShareTarget_NoExtrasIfNoShareData() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);

        doReturn(null).when(mSessionDataHolder).getActiveHandlerForIntent(any());

        Activity spyActivity = spy(mActivity);

        LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(intentCaptor.capture(), any());
        Intent launchedIntent = intentCaptor.getValue();

        assertEquals(
                false,
                launchedIntent.hasExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA));
    }

    @Test
    public void testShareTarget_StripsSpoofedVerifiedData() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);
        ShareData spoofedData =
                new ShareData("title", "text", Arrays.asList(Uri.parse("content://spoofed/data")));
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA, spoofedData.toBundle());

        doReturn(null).when(mSessionDataHolder).getActiveHandlerForIntent(any());

        Activity spyActivity = spy(mActivity);

        LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(intentCaptor.capture(), any());
        Intent launchedIntent = intentCaptor.getValue();

        assertEquals(
                false,
                launchedIntent.hasExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA));
    }

    @Test
    public void testShareTarget_DelegatedToExistingHandler_StripsSpoofedVerifiedData() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);
        ShareData clientData =
                new ShareData("title", "text", Arrays.asList(Uri.parse("content://client/data")));
        intent.putExtra(TrustedWebActivityIntentBuilder.EXTRA_SHARE_DATA, clientData.toBundle());
        ShareData spoofedData =
                new ShareData("title", "text", Arrays.asList(Uri.parse("content://spoofed/data")));
        intent.putExtra(
                CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA, spoofedData.toBundle());

        doReturn(mSessionHandler).when(mSessionDataHolder).getActiveHandlerForIntent(any());
        doReturn(true).when(mSessionHandler).handleIntent(any());
        doReturn(123).when(mSessionHandler).getTaskId();

        Activity spyActivity = spy(mActivity);
        doReturn(mActivityManager).when(spyActivity).getSystemService(Context.ACTIVITY_SERVICE);

        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mSessionHandler).handleIntent(intentCaptor.capture());
        Intent deliveredIntent = intentCaptor.getValue();

        assertFalse(
                deliveredIntent.hasExtra(CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA));
        assertTrue(deliveredIntent.hasExtra(TrustedWebActivityIntentBuilder.EXTRA_SHARE_DATA));
    }

    @Test
    public void testShareTarget_MixedPermissions_StashesOnlyGranted() {
        Uri grantedUri = Uri.parse("content://com.example/granted.jpg");
        Uri deniedUri = Uri.parse("content://com.example/denied.jpg");
        ShareData shareData =
                new ShareData("share_title", "share_text", Arrays.asList(grantedUri, deniedUri));
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        IBinder sessionBinder = mock(IBinder.class);
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, sessionBinder);
        intent.putExtra(TrustedWebActivityIntentBuilder.EXTRA_SHARE_DATA, shareData.toBundle());

        doReturn(null).when(mSessionDataHolder).getActiveHandlerForIntent(any());

        Activity spyActivity = spy(mActivity);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            ComponentCaller mockCaller = mock(ComponentCaller.class);
            doReturn(Process.myUid() + 1).when(mockCaller).getUid();
            doReturn(PackageManager.PERMISSION_GRANTED)
                    .when(mockCaller)
                    .checkContentUriPermission(
                            eq(grantedUri), eq(Intent.FLAG_GRANT_READ_URI_PERMISSION));
            doReturn(PackageManager.PERMISSION_DENIED)
                    .when(mockCaller)
                    .checkContentUriPermission(
                            eq(deniedUri), eq(Intent.FLAG_GRANT_READ_URI_PERMISSION));
            doReturn(mockCaller).when(spyActivity).getInitialCaller();
        } else {
            mockSessionUid();
            doReturn(PackageManager.PERMISSION_GRANTED)
                    .when(spyActivity)
                    .checkUriPermission(
                            eq(grantedUri),
                            eq(TEST_PID),
                            eq(TEST_UID),
                            eq(Intent.FLAG_GRANT_READ_URI_PERMISSION));
            doReturn(PackageManager.PERMISSION_DENIED)
                    .when(spyActivity)
                    .checkUriPermission(
                            eq(deniedUri),
                            eq(TEST_PID),
                            eq(TEST_UID),
                            eq(Intent.FLAG_GRANT_READ_URI_PERMISSION));
        }

        LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(intentCaptor.capture(), any());
        Intent launchedIntent = intentCaptor.getValue();

        Bundle stashedBundle =
                launchedIntent.getBundleExtra(
                        CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        ShareData stashedData = ShareData.fromBundle(stashedBundle);
        assertEquals("share_title", stashedData.title);
        assertEquals("share_text", stashedData.text);
        assertEquals(1, stashedData.uris.size());
        assertEquals(grantedUri, stashedData.uris.get(0));
    }

    @Test
    public void testShareTarget_InvalidUris_StashesEmptyUris() {
        Uri fileUri = Uri.parse("file:///sdcard/file.jpg");
        String authority = "org.chromium.chrome.testprovider";
        Uri internalUri = Uri.parse("content://" + authority + "/file.txt");

        PackageManager pm = mActivity.getPackageManager();
        android.content.pm.PackageInfo packageInfo = new android.content.pm.PackageInfo();
        packageInfo.packageName = mActivity.getPackageName();
        packageInfo.applicationInfo = new ApplicationInfo(mActivity.getApplicationInfo());

        ProviderInfo providerInfo = new ProviderInfo();
        providerInfo.packageName = mActivity.getPackageName();
        providerInfo.authority = authority;

        packageInfo.providers = new ProviderInfo[] {providerInfo};
        org.robolectric.Shadows.shadowOf(pm).installPackage(packageInfo);

        ShareData shareData =
                new ShareData("share_title", "share_text", Arrays.asList(fileUri, internalUri));
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        IBinder sessionBinder = mock(IBinder.class);
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, sessionBinder);
        intent.putExtra(TrustedWebActivityIntentBuilder.EXTRA_SHARE_DATA, shareData.toBundle());

        doReturn(null).when(mSessionDataHolder).getActiveHandlerForIntent(any());

        Activity spyActivity = spy(mActivity);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            ComponentCaller mockCaller = mock(ComponentCaller.class);
            doReturn(Process.myUid() + 1).when(mockCaller).getUid();
            doReturn(PackageManager.PERMISSION_GRANTED)
                    .when(mockCaller)
                    .checkContentUriPermission(any(), eq(Intent.FLAG_GRANT_READ_URI_PERMISSION));
            doReturn(mockCaller).when(spyActivity).getInitialCaller();
        } else {
            mockSessionUid();
            doReturn(PackageManager.PERMISSION_GRANTED)
                    .when(spyActivity)
                    .checkUriPermission(
                            any(), anyInt(), anyInt(), eq(Intent.FLAG_GRANT_READ_URI_PERMISSION));
        }

        LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(intentCaptor.capture(), any());
        Intent launchedIntent = intentCaptor.getValue();

        Bundle stashedBundle =
                launchedIntent.getBundleExtra(
                        CustomTabIntentDataProvider.EXTRA_VERIFIED_SHARE_DATA);
        ShareData stashedData = ShareData.fromBundle(stashedBundle);
        assertEquals("share_title", stashedData.title);
        assertEquals("share_text", stashedData.text);
        assertEquals(0, stashedData.uris.size());
    }

    private static final String GLIC_EXTERNAL_TRIGGERING_ACTION =
            GlicIntentConstants.ACTION_EXTERNAL_TRIGGERING;
    private static final String START_ACTOR_FOREGROUND_SERVICE =
            "org.chromium.chrome.browser.actor.START_ACTOR_FOREGROUND_SERVICE";

    @Test
    public void testDispatchGlicExternalTrigger_WrongAction() {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        Activity spyActivity = spy(mActivity);

        int result = LaunchIntentDispatcher.dispatchGlicExternalTrigger(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.CONTINUE, result);
        verifyNoInteractions(mExternalAuthUtils);
        verifyNoInteractions(mForegroundServiceUtils);
    }

    @Test
    public void testDispatchGlicExternalTrigger_NotGoogleSigned() {
        Intent intent = new Intent(GLIC_EXTERNAL_TRIGGERING_ACTION);
        Activity spyActivity = spy(mActivity);
        doReturn("com.untrusted.app").when(spyActivity).getCallingPackage();
        doReturn(false).when(mExternalAuthUtils).isGoogleSigned("com.untrusted.app");

        int result = LaunchIntentDispatcher.dispatchGlicExternalTrigger(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(spyActivity).setResult(Activity.RESULT_CANCELED);
        verifyNoInteractions(mForegroundServiceUtils);
    }

    @Test
    public void testDispatchGlicExternalTrigger_GlicDisabledForProfile() {
        Intent intent = new Intent(GLIC_EXTERNAL_TRIGGERING_ACTION);
        Activity spyActivity = spy(mActivity);
        doReturn("com.google.android.apps.googlequicksearchbox")
                .when(spyActivity)
                .getCallingPackage();
        doReturn(true)
                .when(mExternalAuthUtils)
                .isGoogleSigned("com.google.android.apps.googlequicksearchbox");
        doReturn(false).when(mGlicEnablingJniMock).isEnabledForProfile(mProfile);

        int result = LaunchIntentDispatcher.dispatchGlicExternalTrigger(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(spyActivity).setResult(Activity.RESULT_CANCELED);
        verifyNoInteractions(mForegroundServiceUtils);
    }

    @Test
    public void testDispatchGlicExternalTrigger_ConsentRequired() {
        Intent intent = new Intent(GLIC_EXTERNAL_TRIGGERING_ACTION);
        Activity spyActivity = spy(mActivity);
        doReturn("com.google.android.apps.googlequicksearchbox")
                .when(spyActivity)
                .getCallingPackage();
        doReturn(true)
                .when(mExternalAuthUtils)
                .isGoogleSigned("com.google.android.apps.googlequicksearchbox");
        doReturn(true).when(mGlicEnablingJniMock).isEnabledForProfile(mProfile);
        doReturn(true).when(mGlicEnablingJniMock).experimentalOptInIsNeeded(mProfile);

        int result = LaunchIntentDispatcher.dispatchGlicExternalTrigger(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(spyActivity).setResult(Activity.RESULT_OK);
        verifyNoInteractions(mForegroundServiceUtils);

        ArgumentCaptor<Intent> launchedIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(launchedIntentCaptor.capture(), any());
        Intent launchedIntent = launchedIntentCaptor.getValue();
        assertNotNull(launchedIntent);
        assertEquals(Intent.ACTION_VIEW, launchedIntent.getAction());
        assertEquals(UrlConstants.NTP_URL, launchedIntent.getDataString());
        assertTrue(launchedIntent.getBooleanExtra(Browser.EXTRA_CREATE_NEW_TAB, false));
        assertTrue(
                launchedIntent.getBooleanExtra(
                        GlicIntentConstants.EXTRA_GLIC_PENDING_ACTOR_TASK, false));
        assertTrue(IntentUtils.isTrustedIntentFromSelf(launchedIntent));
    }

    @Test
    public void testDispatchGlicExternalTrigger_ConsentNotRequired_StartsService() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        Intent intent = new Intent(GLIC_EXTERNAL_TRIGGERING_ACTION);
        Activity spyActivity = spy(mActivity);
        doReturn("com.google.android.apps.googlequicksearchbox")
                .when(spyActivity)
                .getCallingPackage();
        doReturn(true)
                .when(mExternalAuthUtils)
                .isGoogleSigned("com.google.android.apps.googlequicksearchbox");
        doReturn(true).when(mGlicEnablingJniMock).isEnabledForProfile(mProfile);
        doReturn(false).when(mGlicEnablingJniMock).experimentalOptInIsNeeded(mProfile);

        int result = LaunchIntentDispatcher.dispatchGlicExternalTrigger(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(spyActivity).setResult(Activity.RESULT_OK);

        ArgumentCaptor<Intent> serviceIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mForegroundServiceUtils).startForegroundService(serviceIntentCaptor.capture());
        Intent serviceIntent = serviceIntentCaptor.getValue();
        assertEquals(START_ACTOR_FOREGROUND_SERVICE, serviceIntent.getAction());
        assertEquals(
                org.chromium.chrome.browser.actor.ActorForegroundService.class.getName(),
                serviceIntent.getComponent().getClassName());
    }

    @Test
    public void testDispatchGlicExternalTrigger_NotificationsDisabled_OpensNewTab() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        Intent intent = new Intent(GLIC_EXTERNAL_TRIGGERING_ACTION);
        Activity spyActivity = spy(mActivity);
        doReturn("com.google.android.apps.googlequicksearchbox")
                .when(spyActivity)
                .getCallingPackage();
        doReturn(true)
                .when(mExternalAuthUtils)
                .isGoogleSigned("com.google.android.apps.googlequicksearchbox");
        doReturn(true).when(mGlicEnablingJniMock).isEnabledForProfile(mProfile);
        doReturn(false).when(mGlicEnablingJniMock).experimentalOptInIsNeeded(mProfile);

        int result = LaunchIntentDispatcher.dispatchGlicExternalTrigger(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(spyActivity).setResult(Activity.RESULT_OK);
        verifyNoInteractions(mForegroundServiceUtils);

        ArgumentCaptor<Intent> launchedIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(launchedIntentCaptor.capture(), any());
        Intent launchedIntent = launchedIntentCaptor.getValue();
        assertNotNull(launchedIntent);
        assertEquals(Intent.ACTION_VIEW, launchedIntent.getAction());
        assertEquals(UrlConstants.NTP_URL, launchedIntent.getDataString());
        assertTrue(launchedIntent.getBooleanExtra(Browser.EXTRA_CREATE_NEW_TAB, false));
        assertTrue(
                launchedIntent.getBooleanExtra(
                        GlicIntentConstants.EXTRA_GLIC_PENDING_ACTOR_TASK, false));
        assertTrue(IntentUtils.isTrustedIntentFromSelf(launchedIntent));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.GLIC_BACKGROUND_TRIGGERING)
    public void testDispatchGlicExternalTrigger_FeatureDisabled() {
        Intent intent = new Intent(GLIC_EXTERNAL_TRIGGERING_ACTION);
        Activity spyActivity = spy(mActivity);
        doReturn("com.google.android.apps.googlequicksearchbox")
                .when(spyActivity)
                .getCallingPackage();
        doReturn(true)
                .when(mExternalAuthUtils)
                .isGoogleSigned("com.google.android.apps.googlequicksearchbox");
        doReturn(true).when(mGlicEnablingJniMock).isEnabledForProfile(mProfile);
        doReturn(false).when(mGlicEnablingJniMock).experimentalOptInIsNeeded(mProfile);

        int result = LaunchIntentDispatcher.dispatchGlicExternalTrigger(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.CONTINUE, result);
        verifyNoInteractions(mForegroundServiceUtils);
    }

    private static Intent createGlicInterruptIntent(String conversationId) {
        Intent intent = new Intent(GLIC_EXTERNAL_TRIGGERING_ACTION);
        intent.putExtra(GlicIntentConstants.EXTRA_CONVERSATION_ID, conversationId);
        return intent;
    }

    private void setUpTrustedGlicCaller(Activity spyActivity) {
        doReturn("com.google.android.apps.googlequicksearchbox")
                .when(spyActivity)
                .getCallingPackage();
        doReturn(true)
                .when(mExternalAuthUtils)
                .isGoogleSigned("com.google.android.apps.googlequicksearchbox");
        doReturn(true).when(mGlicEnablingJniMock).isEnabledForProfile(mProfile);
    }

    private ActorTask setUpGlicInterrupt(
            Activity spyActivity, String conversationId, int taskId, @TabId int targetTabId) {
        setUpTrustedGlicCaller(spyActivity);

        ActorKeyedService actorKeyedService = mock(ActorKeyedService.class);
        ActorKeyedServiceFactory.setForTesting(actorKeyedService);

        ActorTask task = mock(ActorTask.class);
        when(task.getId()).thenReturn(taskId);
        when(task.getTargetTabId()).thenReturn(targetTabId);
        when(task.getGlicConversationId()).thenReturn(conversationId);
        when(actorKeyedService.getTaskByConversationId(conversationId)).thenReturn(task);
        return task;
    }

    private ActorForegroundServiceController setUpInterruptController(ActorTask task) {
        ActorForegroundServiceController controller = mock(ActorForegroundServiceController.class);
        ActorForegroundServiceController.setInstanceForTesting(controller);
        Intent bringToFront =
                IntentHandler.createTrustedBringTabToFrontIntent(
                        task.getTargetTabId(), IntentHandler.BringToFrontSource.NOTIFICATION);
        bringToFront.putExtra(ActorNotificationFactory.EXTRA_SHOW_ACTOR_CONTROL, true);
        bringToFront.putExtra(
                GlicIntentConstants.EXTRA_CONVERSATION_ID, task.getGlicConversationId());
        bringToFront.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_ID, task.getId());
        bringToFront.putExtra(NotificationConstants.EXTRA_ACTOR_TASK_STATE, task.getState());
        when(controller.createTrustedBringTabToFrontIntent(task)).thenReturn(bringToFront);
        return controller;
    }

    @Test
    public void testDispatchGlicExternalTrigger_WithConversationId_RoutesToTabbedActivity() {
        Intent intent = createGlicInterruptIntent("conv_123");
        Activity spyActivity = spy(mActivity);
        ActorTask task = setUpGlicInterrupt(spyActivity, "conv_123", /* taskId= */ 456, 789);
        ActorForegroundServiceController controller = setUpInterruptController(task);

        int result = LaunchIntentDispatcher.dispatchGlicExternalTrigger(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(spyActivity).setResult(Activity.RESULT_OK);
        verify(controller).createTrustedBringTabToFrontIntent(task);
        verifyNoInteractions(mForegroundServiceUtils);

        ArgumentCaptor<Intent> launchedIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(launchedIntentCaptor.capture());
        Intent launchedIntent = launchedIntentCaptor.getValue();
        assertTrue(IntentHandler.wasIntentSenderChrome(launchedIntent));
        assertEquals("conv_123", IntentHandler.getGlicConversationId(launchedIntent));
        assertEquals(789, IntentHandler.getBringTabToFrontId(launchedIntent));
        assertEquals(
                456,
                IntentUtils.safeGetIntExtra(
                        launchedIntent, NotificationConstants.EXTRA_ACTOR_TASK_ID, -1));
        assertTrue(
                IntentUtils.safeGetBooleanExtra(
                        launchedIntent, ActorNotificationFactory.EXTRA_SHOW_ACTOR_CONTROL, false));
        assertFalse(
                "No window could be resolved, so none should be stamped.",
                launchedIntent.hasExtra(IntentHandler.EXTRA_WINDOW_ID));
    }

    @Test
    public void
            testDispatchGlicExternalTrigger_WithConversationId_TaskNotFound_FallsBackGracefully() {
        Intent intent = createGlicInterruptIntent("conv_123");
        Activity spyActivity = spy(mActivity);
        setUpTrustedGlicCaller(spyActivity);

        ActorKeyedService actorKeyedService = mock(ActorKeyedService.class);
        ActorKeyedServiceFactory.setForTesting(actorKeyedService);
        when(actorKeyedService.getTaskByConversationId("conv_123")).thenReturn(null);

        int result = LaunchIntentDispatcher.dispatchGlicExternalTrigger(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(spyActivity).setResult(Activity.RESULT_CANCELED);
        verify(spyActivity, never()).startActivity(any());
        verifyNoInteractions(mForegroundServiceUtils);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ACTOR_NOTIFICATION_INTENT_ROUTING)
    public void testDispatchGlicExternalTrigger_WithConversationId_RoutingDisabled_StartsService() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        Intent intent = createGlicInterruptIntent("conv_123");
        Activity spyActivity = spy(mActivity);
        setUpTrustedGlicCaller(spyActivity);
        doReturn(false).when(mGlicEnablingJniMock).experimentalOptInIsNeeded(mProfile);

        int result = LaunchIntentDispatcher.dispatchGlicExternalTrigger(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(spyActivity).setResult(Activity.RESULT_OK);
        verify(spyActivity, never()).startActivity(any());
        verify(mForegroundServiceUtils).startForegroundService(any());
    }

    @Test
    public void testDispatchGlicExternalTrigger_WithConversationId_ConsentRequired_OpensNewTab() {
        Intent intent = createGlicInterruptIntent("conv_123");
        Activity spyActivity = spy(mActivity);
        setUpTrustedGlicCaller(spyActivity);
        doReturn(true).when(mGlicEnablingJniMock).experimentalOptInIsNeeded(mProfile);

        int result = LaunchIntentDispatcher.dispatchGlicExternalTrigger(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(spyActivity).setResult(Activity.RESULT_OK);
        verifyNoInteractions(mForegroundServiceUtils);

        ArgumentCaptor<Intent> launchedIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(launchedIntentCaptor.capture(), any());
        Intent launchedIntent = launchedIntentCaptor.getValue();
        assertNotNull(launchedIntent);
        assertEquals(Intent.ACTION_VIEW, launchedIntent.getAction());
        assertEquals(UrlConstants.NTP_URL, launchedIntent.getDataString());
        assertTrue(launchedIntent.getBooleanExtra(Browser.EXTRA_CREATE_NEW_TAB, false));
        assertTrue(
                launchedIntent.getBooleanExtra(
                        GlicIntentConstants.EXTRA_GLIC_PENDING_ACTOR_TASK, false));
        assertTrue(IntentUtils.isTrustedIntentFromSelf(launchedIntent));
    }

    @Test
    public void testDispatchGlicExternalTrigger_TabInOtherWindow_RoutesToThatWindow() {
        MultiWindowTestUtils.enableMultiInstance();
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 5,
                /* url= */ "https://www.example.com",
                /* tabCount= */ 1,
                /* taskId= */ 57);

        Intent intent = createGlicInterruptIntent("conv_123");
        Activity spyActivity = spy(mActivity);
        ActorTask task = setUpGlicInterrupt(spyActivity, "conv_123", /* taskId= */ 456, 789);
        setUpInterruptController(task);

        TabModel tabModel = mock(TabModel.class);
        when(tabModel.getTabModelType()).thenReturn(TabModelType.STANDARD);
        TabWindowInfo tabWindowInfo =
                new TabWindowInfo(
                        /* windowId= */ 5, mock(TabModelSelector.class), tabModel, mock(Tab.class));
        TabWindowManager tabWindowManager = mock(TabWindowManager.class);
        when(tabWindowManager.getTabWindowInfoById(789)).thenReturn(tabWindowInfo);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(tabWindowManager);

        int result = LaunchIntentDispatcher.dispatchGlicExternalTrigger(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(spyActivity).setResult(Activity.RESULT_OK);
        ArgumentCaptor<Intent> launchedIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(launchedIntentCaptor.capture());
        assertEquals(
                "The window hosting the tab should be reused.",
                5,
                IntentUtils.safeGetIntExtra(
                        launchedIntentCaptor.getValue(),
                        IntentHandler.EXTRA_WINDOW_ID,
                        TabWindowManager.INVALID_WINDOW_ID));
    }

    @Test
    public void testDispatchGlicExternalTrigger_TabInInactiveWindow_StampsNoWindow() {
        MultiWindowTestUtils.enableMultiInstance();
        // The instance is still persisted, but its task is gone from Android Recents.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 5,
                /* url= */ "https://www.example.com",
                /* tabCount= */ 1,
                /* taskId= */ -1);

        Intent intent = createGlicInterruptIntent("conv_123");
        Activity spyActivity = spy(mActivity);
        ActorTask task = setUpGlicInterrupt(spyActivity, "conv_123", /* taskId= */ 456, 789);
        setUpInterruptController(task);

        TabModel tabModel = mock(TabModel.class);
        when(tabModel.getTabModelType()).thenReturn(TabModelType.STANDARD);
        TabWindowInfo tabWindowInfo =
                new TabWindowInfo(
                        /* windowId= */ 5, mock(TabModelSelector.class), tabModel, mock(Tab.class));
        TabWindowManager tabWindowManager = mock(TabWindowManager.class);
        when(tabWindowManager.getTabWindowInfoById(789)).thenReturn(tabWindowInfo);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(tabWindowManager);

        int result = LaunchIntentDispatcher.dispatchGlicExternalTrigger(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(spyActivity).setResult(Activity.RESULT_OK);
        ArgumentCaptor<Intent> launchedIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(launchedIntentCaptor.capture());
        assertFalse(
                "A window with no live task should not be resurrected.",
                launchedIntentCaptor.getValue().hasExtra(IntentHandler.EXTRA_WINDOW_ID));
    }

    @Test
    public void testDispatchGlicExternalTrigger_BackgroundTask_RoutesToRecordedWindow() {
        MultiWindowTestUtils.enableMultiInstance();
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 3,
                /* url= */ "https://www.example.com",
                /* tabCount= */ 1,
                /* taskId= */ 57);

        Intent intent = createGlicInterruptIntent("conv_123");
        Activity spyActivity = spy(mActivity);
        ActorTask task =
                setUpGlicInterrupt(spyActivity, "conv_123", /* taskId= */ 456, Tab.INVALID_TAB_ID);
        ActorForegroundServiceController controller = setUpInterruptController(task);
        when(controller.getWindowIdForTask(456)).thenReturn(3);

        int result = LaunchIntentDispatcher.dispatchGlicExternalTrigger(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(spyActivity).setResult(Activity.RESULT_OK);
        ArgumentCaptor<Intent> launchedIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(launchedIntentCaptor.capture());
        assertEquals(
                "The window recorded for the background task should be reused.",
                3,
                IntentUtils.safeGetIntExtra(
                        launchedIntentCaptor.getValue(),
                        IntentHandler.EXTRA_WINDOW_ID,
                        TabWindowManager.INVALID_WINDOW_ID));
    }

    @Test
    public void testDispatchGlicExternalTrigger_BackgroundTask_ClosedWindow_StampsNoWindow() {
        MultiWindowTestUtils.enableMultiInstance();

        Intent intent = createGlicInterruptIntent("conv_123");
        Activity spyActivity = spy(mActivity);
        ActorTask task =
                setUpGlicInterrupt(spyActivity, "conv_123", /* taskId= */ 456, Tab.INVALID_TAB_ID);
        ActorForegroundServiceController controller = setUpInterruptController(task);
        // The window was closed after the task was handed off to background actuation.
        when(controller.getWindowIdForTask(456)).thenReturn(3);

        int result = LaunchIntentDispatcher.dispatchGlicExternalTrigger(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(spyActivity).setResult(Activity.RESULT_OK);
        ArgumentCaptor<Intent> launchedIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(launchedIntentCaptor.capture());
        assertFalse(
                "A closed window should not be resurrected.",
                launchedIntentCaptor.getValue().hasExtra(IntentHandler.EXTRA_WINDOW_ID));
    }

    @Test
    public void testDispatchGlicExternalTrigger_BackgroundTask_InactiveWindow_StampsNoWindow() {
        MultiWindowTestUtils.enableMultiInstance();
        // The instance is still persisted, but its task is gone from Android Recents.
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 3,
                /* url= */ "https://www.example.com",
                /* tabCount= */ 1,
                /* taskId= */ -1);

        Intent intent = createGlicInterruptIntent("conv_123");
        Activity spyActivity = spy(mActivity);
        ActorTask task =
                setUpGlicInterrupt(spyActivity, "conv_123", /* taskId= */ 456, Tab.INVALID_TAB_ID);
        ActorForegroundServiceController controller = setUpInterruptController(task);
        when(controller.getWindowIdForTask(456)).thenReturn(3);

        int result = LaunchIntentDispatcher.dispatchGlicExternalTrigger(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(spyActivity).setResult(Activity.RESULT_OK);
        ArgumentCaptor<Intent> launchedIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(launchedIntentCaptor.capture());
        assertFalse(
                "A window with no live task should not be resurrected.",
                launchedIntentCaptor.getValue().hasExtra(IntentHandler.EXTRA_WINDOW_ID));
    }

    @Test
    public void testDispatchGlicExternalTrigger_LaunchedInExistingInstance_SkipsNewLaunch() {
        MultiWindowTestUtils.enableMultiInstance();
        MultiWindowTestUtils.createInstance(
                /* instanceId= */ 3,
                /* url= */ "https://www.example.com",
                /* tabCount= */ 1,
                /* taskId= */ 57);
        MultiWindowUtils.setActivityByWindowIdForTesting(3, mock(ChromeTabbedActivity.class));
        AndroidTaskUtils.setAppTaskForTesting(mock(AppTask.class));

        Intent intent = createGlicInterruptIntent("conv_123");
        Activity spyActivity = spy(mActivity);
        ActorTask task =
                setUpGlicInterrupt(spyActivity, "conv_123", /* taskId= */ 456, Tab.INVALID_TAB_ID);
        ActorForegroundServiceController controller = setUpInterruptController(task);
        when(controller.getWindowIdForTask(456)).thenReturn(3);

        int result = LaunchIntentDispatcher.dispatchGlicExternalTrigger(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(spyActivity).setResult(Activity.RESULT_OK);
        verify(spyActivity, never()).startActivity(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ALWAYS_OPEN_IN_BROWSER)
    public void testDispatchToCustomTabActivity_AlwaysOpenInBrowser_OverridesToTabbedActivity() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CUSTOM_TABS_ALWAYS_OPEN_IN_BROWSER, true);

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);

        Activity spyActivity = spy(mActivity);
        UserActionTester userActionTester = new UserActionTester();
        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(captor.capture());
        assertEquals(
                ChromeTabbedActivity.class.getName(),
                captor.getValue().getComponent().getClassName());
        assertEquals(1, userActionTester.getActionCount("CustomTabs.AlwaysOpenInBrowserOverride"));
        userActionTester.tearDown();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CCT_ALWAYS_OPEN_IN_BROWSER)
    public void testDispatchToCustomTabActivity_AlwaysOpenInBrowser_FeatureDisabled() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CUSTOM_TABS_ALWAYS_OPEN_IN_BROWSER, true);

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);

        Activity spyActivity = spy(mActivity);
        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(captor.capture(), any());
        assertEquals(
                CustomTabActivity.class.getName(), captor.getValue().getComponent().getClassName());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ALWAYS_OPEN_IN_BROWSER)
    public void testDispatchToCustomTabActivity_AlwaysOpenInBrowser_PrefDisabled() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CUSTOM_TABS_ALWAYS_OPEN_IN_BROWSER, false);

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);

        Activity spyActivity = spy(mActivity);
        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(captor.capture(), any());
        assertEquals(
                CustomTabActivity.class.getName(), captor.getValue().getComponent().getClassName());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ALWAYS_OPEN_IN_BROWSER)
    public void testDispatchToCustomTabActivity_AlwaysOpenInBrowser_FromChrome() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CUSTOM_TABS_ALWAYS_OPEN_IN_BROWSER, true);

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);
        intent.setPackage(mActivity.getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);

        Activity spyActivity = spy(mActivity);
        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(captor.capture(), any());
        assertEquals(
                CustomTabActivity.class.getName(), captor.getValue().getComponent().getClassName());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ALWAYS_OPEN_IN_BROWSER)
    public void testDispatchToCustomTabActivity_AlwaysOpenInBrowser_AuthTab() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CUSTOM_TABS_ALWAYS_OPEN_IN_BROWSER, true);

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(AuthTabIntent.EXTRA_LAUNCH_AUTH_TAB, true);

        Activity spyActivity = spy(mActivity);
        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(captor.capture(), any());
        assertEquals(
                CustomTabActivity.class.getName(), captor.getValue().getComponent().getClassName());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ALWAYS_OPEN_IN_BROWSER)
    public void testDispatchToCustomTabActivity_AlwaysOpenInBrowser_Twa() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CUSTOM_TABS_ALWAYS_OPEN_IN_BROWSER, true);

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);
        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true);

        Activity spyActivity = spy(mActivity);
        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(captor.capture(), any());
        assertEquals(
                CustomTabActivity.class.getName(), captor.getValue().getComponent().getClassName());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.CCT_ALWAYS_OPEN_IN_BROWSER,
        ChromeFeatureList.CCT_INCOGNITO_AVAILABLE_TO_THIRD_PARTY
    })
    public void testDispatchToCustomTabActivity_AlwaysOpenInBrowser_Incognito() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CUSTOM_TABS_ALWAYS_OPEN_IN_BROWSER, true);

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        intent.putExtra(IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE, "com.example");

        Activity spyActivity = spy(mActivity);
        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(captor.capture(), any());
        assertEquals(
                CustomTabActivity.class.getName(), captor.getValue().getComponent().getClassName());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ALWAYS_OPEN_IN_BROWSER)
    public void testDispatchToCustomTabActivity_AlwaysOpenInBrowser_NonDefaultUiType() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CUSTOM_TABS_ALWAYS_OPEN_IN_BROWSER, true);

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.MEDIA_VIEWER);

        Activity spyActivity = spy(mActivity);
        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(captor.capture(), any());
        assertEquals(
                CustomTabActivity.class.getName(), captor.getValue().getComponent().getClassName());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CCT_ALWAYS_OPEN_IN_BROWSER)
    public void testDispatchToCustomTabActivity_AlwaysOpenInBrowser_NetworkBound() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.CUSTOM_TABS_ALWAYS_OPEN_IN_BROWSER, true);

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);
        android.net.Network mockNetwork = mock(android.net.Network.class);
        doReturn(mockNetwork).when(mCustomTabsConnection).extractTargetNetwork(any(), any());

        Activity spyActivity = spy(mActivity);
        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(captor.capture(), any());
        assertEquals(
                CustomTabActivity.class.getName(), captor.getValue().getComponent().getClassName());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.GLIC_BACKGROUND_ACTUATION)
    public void testDispatchGlicExternalTrigger_BackgroundActuationDisabled_OpensNewTab() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        Intent intent = new Intent(GLIC_EXTERNAL_TRIGGERING_ACTION);
        Activity spyActivity = spy(mActivity);
        doReturn("com.google.android.apps.googlequicksearchbox")
                .when(spyActivity)
                .getCallingPackage();
        doReturn(true)
                .when(mExternalAuthUtils)
                .isGoogleSigned("com.google.android.apps.googlequicksearchbox");
        doReturn(true).when(mGlicEnablingJniMock).isEnabledForProfile(mProfile);
        doReturn(false).when(mGlicEnablingJniMock).experimentalOptInIsNeeded(mProfile);

        int result = LaunchIntentDispatcher.dispatchGlicExternalTrigger(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.FINISH_ACTIVITY, result);
        verify(spyActivity).setResult(Activity.RESULT_OK);
        verifyNoInteractions(mForegroundServiceUtils);

        ArgumentCaptor<Intent> launchedIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(launchedIntentCaptor.capture(), any());
        Intent launchedIntent = launchedIntentCaptor.getValue();
        assertNotNull(launchedIntent);
        assertEquals(Intent.ACTION_VIEW, launchedIntent.getAction());
        assertEquals(UrlConstants.NTP_URL, launchedIntent.getDataString());
        assertTrue(launchedIntent.getBooleanExtra(Browser.EXTRA_CREATE_NEW_TAB, false));
        assertTrue(
                launchedIntent.getBooleanExtra(
                        GlicIntentConstants.EXTRA_GLIC_PENDING_ACTOR_TASK, false));
        assertTrue(IntentUtils.isTrustedIntentFromSelf(launchedIntent));
    }

    @Test
    public void testDispatchToCustomTabActivity_DoesNotLaunchJavaScriptUrls() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("javascript: alert('Hello');"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);
        Activity spyActivity = spy(mActivity);

        int result = LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        assertEquals(LaunchIntentDispatcher.Action.CONTINUE, result);
        verify(spyActivity, never()).startActivity(any(), any());
    }

    @Test
    public void testDispatchToCustomTabActivity_CallingActivityExtra() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);
        intent.putExtra(IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE, "spoofed");
        Activity spyActivity = spy(mActivity);
        doReturn("com.foo.bar").when(spyActivity).getCallingPackage();

        LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(captor.capture(), any());
        assertEquals(
                "com.foo.bar",
                captor.getValue().getStringExtra(IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE));
    }

    @Test
    public void testDispatchToCustomTabActivity_CallingActivityExtra_NotSet() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://example.com"));
        intent.putExtra(CustomTabsIntent.EXTRA_SESSION, (IBinder) null);
        intent.putExtra(IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE, "spoofed");
        Activity spyActivity = spy(mActivity);
        doReturn(null).when(spyActivity).getCallingPackage();

        LaunchIntentDispatcher.dispatchToCustomTabActivity(spyActivity, intent);

        ArgumentCaptor<Intent> captor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(captor.capture(), any());
        assertNull(captor.getValue().getStringExtra(IntentHandler.EXTRA_CALLING_ACTIVITY_PACKAGE));
    }
}
