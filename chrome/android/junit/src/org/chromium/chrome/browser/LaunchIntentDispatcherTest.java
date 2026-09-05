// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ComponentCaller;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Process;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.trusted.FileHandlingData;
import androidx.browser.trusted.TrustedWebActivityIntentBuilder;
import androidx.browser.trusted.sharing.ShareData;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.SessionDataHolder;
import org.chromium.chrome.browser.browserservices.SessionHandler;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicEnablingJni;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.browser_ui.notifications.ForegroundServiceUtils;
import org.chromium.components.externalauth.ExternalAuthUtils;

import java.util.Arrays;

/** Unit tests for {@link LaunchIntentDispatcher}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.CCT_DONT_OVERRIDE_INTENT_MIME_TYPE,
    ChromeFeatureList.GLIC_BACKGROUND_TRIGGERING
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
            "org.chromium.chrome.browser.glic.EXTERNAL_TRIGGERING";
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

        assertEquals(LaunchIntentDispatcher.Action.CONTINUE, result);
        verifyNoInteractions(mForegroundServiceUtils);
    }

    @Test
    public void testDispatchGlicExternalTrigger_ConsentNotRequired_StartsService() {
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
}
