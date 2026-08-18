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
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Process;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.trusted.FileHandlingData;
import androidx.browser.trusted.TrustedWebActivityIntentBuilder;

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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.SessionDataHolder;
import org.chromium.chrome.browser.browserservices.SessionHandler;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.Arrays;

/** Unit tests for {@link LaunchIntentDispatcher}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.CCT_DONT_OVERRIDE_INTENT_MIME_TYPE)
public class LaunchIntentDispatcherTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private CustomTabsConnection mCustomTabsConnection;
    @Mock private SessionDataHolder mSessionDataHolder;
    @Mock private SessionHandler mSessionHandler;
    @Mock private ActivityManager mActivityManager;
    @Mock IntentHandler.Natives mIntentHandlerNativeMock;
    @Mock ExternalIntentUrlChecker.Natives mExternalIntentUrlCheckerNativeMock;

    private Activity mActivity;

    @Before
    public void setUp() {
        ExternalIntentUrlCheckerJni.setInstanceForTesting(mExternalIntentUrlCheckerNativeMock);
        doReturn(true).when(mExternalIntentUrlCheckerNativeMock).validateUrl(any());
        IntentHandlerJni.setInstanceForTesting(mIntentHandlerNativeMock);

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
}
