// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.Manifest;
import android.app.Notification;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;

import org.chromium.base.ContextUtils;
import org.chromium.base.SplitCompatService;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.ForegroundServiceUtils;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.media.capture.ScreenCapture;
import org.chromium.url.GURL;

import java.util.Set;

/** Unit tests for {@link MediaCaptureNotificationServiceImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ContentFeatureList.ANDROID_ENABLE_BACKGROUND_MEDIA_CAPTURING,
    ChromeFeatureList.MEDIA_CAPTURE_NOTIFICATION_REFRESH_MEDIA_TYPES
})
public class MediaCaptureNotificationServiceImplTest {
    private static final int TAB_ID_1 = 1;
    private static final int NOTIFICATION_ID_1 = TAB_ID_1 + 1;
    private static final int TAB_ID_2 = 2;
    private static final int NOTIFICATION_ID_2 = TAB_ID_2 + 1;
    private static final GURL TEST_URL = new GURL("https://example.com");

    private static class MockServiceImpl extends MediaCaptureNotificationServiceImpl {
        void setServiceForTesting(SplitCompatService service) {
            setService(service);
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mMockContext;
    @Mock private SplitCompatService mMockService;
    @Mock private ForegroundServiceUtils mMockForegroundServiceUtils;
    @Mock private BaseNotificationManagerProxy mMockNotificationManager;
    @Mock private TabWindowManager mMockTabWindowManager;
    @Mock private Tab mMockTab1;
    @Mock private Tab mMockTab2;
    @Mock private WebContents mMockWebContents1;
    @Mock private WebContents mMockWebContents2;
    @Mock private MediaCaptureDevicesDispatcherAndroid.Natives mMediaCaptureJniMock;

    private MockServiceImpl mService;

    @Before
    public void setUp() {
        ForegroundServiceUtils.setInstanceForTesting(mMockForegroundServiceUtils);
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mMockNotificationManager);
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mMockTabWindowManager);
        MediaCaptureDevicesDispatcherAndroidJni.setInstanceForTesting(mMediaCaptureJniMock);
        ScreenCapture.resetStaticStateForTesting();

        lenient().when(mMockTabWindowManager.getTabById(TAB_ID_1)).thenReturn(mMockTab1);
        lenient().when(mMockTab1.getWebContents()).thenReturn(mMockWebContents1);
        lenient().when(mMockTabWindowManager.getTabById(TAB_ID_2)).thenReturn(mMockTab2);
        lenient().when(mMockTab2.getWebContents()).thenReturn(mMockWebContents2);
        lenient()
                .when(mMockService.getPackageName())
                .thenReturn(ContextUtils.getApplicationContext().getPackageName());
        lenient()
                .when(
                        mMockService.checkPermission(
                                eq(Manifest.permission.CAMERA), anyInt(), anyInt()))
                .thenReturn(PackageManager.PERMISSION_GRANTED);
        lenient()
                .when(
                        mMockService.checkPermission(
                                eq(Manifest.permission.RECORD_AUDIO), anyInt(), anyInt()))
                .thenReturn(PackageManager.PERMISSION_GRANTED);

        mService = new MockServiceImpl();
        mService.setServiceForTesting(mMockService);
        mService.onCreate();
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.MEDIA_WEBRTC_NOTIFICATION_IDS);
        ScreenCapture.resetStaticStateForTesting();
    }

    private Intent captureUpdateIntent(int tabId, @Nullable WebContents webContents) {
        MediaCaptureNotificationServiceImpl.updateMediaNotificationForTab(
                mMockContext, tabId, webContents, TEST_URL);
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mMockContext).startService(intentCaptor.capture());
        clearInvocations(mMockContext);
        return intentCaptor.getValue();
    }

    private Intent captureStopIntent(int notificationId) {
        ArgumentCaptor<Notification> notificationCaptor =
                ArgumentCaptor.forClass(Notification.class);
        verify(mMockForegroundServiceUtils)
                .startForeground(
                        eq(mMockService),
                        eq(notificationId),
                        notificationCaptor.capture(),
                        anyInt());
        Notification notification = notificationCaptor.getValue();
        assertNotNull(notification.actions);
        return Shadows.shadowOf(notification.actions[0].actionIntent).getSavedIntent();
    }

    @Test
    public void testNormalScreenCaptureLifecycle_startsAndStopsForegroundService() {
        when(mMediaCaptureJniMock.isCapturingScreen(mMockWebContents1)).thenReturn(true);

        int expectedFgsType =
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CAMERA
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION;

        mService.onStartCommand(
                captureUpdateIntent(TAB_ID_1, mMockWebContents1), /* flags= */ 0, /* startId= */ 1);

        verify(mMockForegroundServiceUtils)
                .startForeground(
                        eq(mMockService), eq(NOTIFICATION_ID_1), any(), eq(expectedFgsType));
        Set<String> trackedIds =
                ChromeSharedPreferences.getInstance()
                        .readStringSet(ChromePreferenceKeys.MEDIA_WEBRTC_NOTIFICATION_IDS, null);
        assertNotNull(trackedIds);
        assertTrue(trackedIds.contains(String.valueOf(NOTIFICATION_ID_1)));

        // Stop capture normally.
        when(mMediaCaptureJniMock.isCapturingScreen(mMockWebContents1)).thenReturn(false);
        mService.onStartCommand(
                captureUpdateIntent(TAB_ID_1, mMockWebContents1), /* flags= */ 0, /* startId= */ 2);

        verify(mMockForegroundServiceUtils)
                .stopForeground(eq(mMockService), eq(Service.STOP_FOREGROUND_REMOVE));
        verify(mMockService).stopSelf(2);
    }

    @Test
    public void testNormalTabCaptureLifecycle_includesMediaPlaybackAndMediaProjection() {
        when(mMediaCaptureJniMock.isCapturingTab(mMockWebContents1)).thenReturn(true);

        int expectedFgsType =
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CAMERA
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION;

        mService.onStartCommand(
                captureUpdateIntent(TAB_ID_1, mMockWebContents1), /* flags= */ 0, /* startId= */ 1);

        verify(mMockForegroundServiceUtils)
                .startForeground(
                        eq(mMockService), eq(NOTIFICATION_ID_1), any(), eq(expectedFgsType));
    }

    @Test
    @DisableFeatures(ContentFeatureList.ANDROID_ENABLE_BACKGROUND_MEDIA_CAPTURING)
    public void testNonBackgroundMediaCapturingMode_usesNotificationManagerAndStickyCleanup() {
        when(mMediaCaptureJniMock.isCapturingAudio(mMockWebContents1)).thenReturn(true);

        mService.onStartCommand(
                captureUpdateIntent(TAB_ID_1, mMockWebContents1), /* flags= */ 0, /* startId= */ 1);

        verify(mMockForegroundServiceUtils, never())
                .startForeground(any(), anyInt(), any(), anyInt());
        ArgumentCaptor<NotificationWrapper> notificationCaptor =
                ArgumentCaptor.forClass(NotificationWrapper.class);
        verify(mMockNotificationManager).notify(notificationCaptor.capture());

        // Simulate sticky service restart with null intent after process crash.
        mService.onStartCommand(null, /* flags= */ 0, /* startId= */ 2);

        verify(mMockNotificationManager)
                .cancel(notificationCaptor.getValue().getMetadata().tag, NOTIFICATION_ID_1);
        assertNull(
                ChromeSharedPreferences.getInstance()
                        .readStringSet(ChromePreferenceKeys.MEDIA_WEBRTC_NOTIFICATION_IDS, null));
        verify(mMockService).stopSelf();
    }

    @Test
    public void testActionScreenCaptureStop_stopsTabAndDisplayCapture() {
        TabSharingUiManager manager = new TabSharingUiManager();
        TabSharingUiManager.setInstanceForTesting(manager);
        TabSharingUiBridge bridge = mock(TabSharingUiBridge.class);
        when(bridge.getCapturer()).thenReturn(mMockWebContents1);
        manager.addBridge(bridge);

        when(mMediaCaptureJniMock.isCapturingTab(mMockWebContents1)).thenReturn(true);
        mService.onStartCommand(
                captureUpdateIntent(TAB_ID_1, mMockWebContents1), /* flags= */ 0, /* startId= */ 1);

        mService.onStartCommand(
                captureStopIntent(NOTIFICATION_ID_1), /* flags= */ 0, /* startId= */ 2);
        verify(bridge).stopSharing();
        verify(mMediaCaptureJniMock, never()).notifyDisplayMediaStopped(mMockWebContents1);

        // Now test display capture stop routing on Tab 2.
        when(mMediaCaptureJniMock.isCapturingScreen(mMockWebContents2)).thenReturn(true);
        mService.onStartCommand(
                captureUpdateIntent(TAB_ID_2, mMockWebContents2), /* flags= */ 0, /* startId= */ 3);

        mService.onStartCommand(
                captureStopIntent(NOTIFICATION_ID_2), /* flags= */ 0, /* startId= */ 4);
        verify(mMediaCaptureJniMock).notifyDisplayMediaStopped(mMockWebContents2);
    }

    @Test
    public void testMultiTabNotifications_updatesForegroundServiceWhenLatestNotificationRemoved() {
        when(mMediaCaptureJniMock.isCapturingAudio(mMockWebContents1)).thenReturn(true);
        when(mMediaCaptureJniMock.isCapturingScreen(mMockWebContents2)).thenReturn(true);

        int fgsAudioOnly =
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CAMERA
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE;
        int fgsWithProjection = fgsAudioOnly | ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION;

        // Tab 1 starts audio call.
        mService.onStartCommand(
                captureUpdateIntent(TAB_ID_1, mMockWebContents1), /* flags= */ 0, /* startId= */ 1);
        verify(mMockForegroundServiceUtils)
                .startForeground(eq(mMockService), eq(NOTIFICATION_ID_1), any(), eq(fgsAudioOnly));

        // Tab 2 starts screen capture.
        mService.onStartCommand(
                captureUpdateIntent(TAB_ID_2, mMockWebContents2), /* flags= */ 0, /* startId= */ 2);
        verify(mMockForegroundServiceUtils)
                .startForeground(
                        eq(mMockService), eq(NOTIFICATION_ID_2), any(), eq(fgsWithProjection));

        clearInvocations(mMockForegroundServiceUtils);

        // Tab 2 stops screen capture; service should fall back to Tab 1's notification without
        // MEDIA_PROJECTION.
        when(mMediaCaptureJniMock.isCapturingScreen(mMockWebContents2)).thenReturn(false);
        mService.onStartCommand(
                captureUpdateIntent(TAB_ID_2, mMockWebContents2), /* flags= */ 0, /* startId= */ 3);

        verify(mMockForegroundServiceUtils)
                .startForeground(eq(mMockService), eq(NOTIFICATION_ID_1), any(), eq(fgsAudioOnly));
        verify(mMockService, never()).stopSelf(anyInt());
    }

    @Test
    public void testUpdateNotification_endOfCallMultiStreamTeardownRace() {
        // 1. Call + screen capture are both active on Tab 1.
        when(mMediaCaptureJniMock.isCapturingScreen(mMockWebContents1)).thenReturn(true);
        when(mMediaCaptureJniMock.isCapturingAudio(mMockWebContents1)).thenReturn(true);
        when(mMediaCaptureJniMock.isCapturingVideo(mMockWebContents1)).thenReturn(true);

        mService.onStartCommand(
                captureUpdateIntent(TAB_ID_1, mMockWebContents1), /* flags= */ 0, /* startId= */ 1);
        clearInvocations(mMockForegroundServiceUtils, mMockService);

        // 2. Call ends: AUDIO_AND_VIDEO stops first and queues an update with [SCREEN_CAPTURE],
        // then SCREEN_CAPTURE also stops and queues the final empty update before the service
        // processes the first queued intent.
        when(mMediaCaptureJniMock.isCapturingAudio(mMockWebContents1)).thenReturn(false);
        when(mMediaCaptureJniMock.isCapturingVideo(mMockWebContents1)).thenReturn(false);
        Intent staleScreenOnlyIntent = captureUpdateIntent(TAB_ID_1, mMockWebContents1);

        when(mMediaCaptureJniMock.isCapturingScreen(mMockWebContents1)).thenReturn(false);
        Intent finalEmptyIntent = captureUpdateIntent(TAB_ID_1, mMockWebContents1);

        mService.onStartCommand(staleScreenOnlyIntent, /* flags= */ 0, /* startId= */ 2);

        // Must NOT call startForeground with the revoked MEDIA_PROJECTION token; instead, it should
        // tear down the notification and stop the foreground service.
        verify(mMockForegroundServiceUtils, never())
                .startForeground(any(), anyInt(), any(), anyInt());
        verify(mMockForegroundServiceUtils)
                .stopForeground(eq(mMockService), eq(Service.STOP_FOREGROUND_REMOVE));
        verify(mMockService).stopSelf(2);

        // 3. The final empty intent arrives right after and is a clean no-op.
        clearInvocations(mMockForegroundServiceUtils, mMockService);
        mService.onStartCommand(finalEmptyIntent, /* flags= */ 0, /* startId= */ 3);
        verify(mMockForegroundServiceUtils, never())
                .startForeground(any(), anyInt(), any(), anyInt());
        verify(mMockService).stopSelf(3);
    }

    @Test
    public void testUpdateNotification_staleMediaTypesReplacedByLiveWebContents() {
        // Intent snapshot contains both SCREEN_CAPTURE and AUDIO_AND_VIDEO, but screen capture has
        // already stopped on the live WebContents by the time onStartCommand runs.
        when(mMediaCaptureJniMock.isCapturingScreen(mMockWebContents1)).thenReturn(true);
        when(mMediaCaptureJniMock.isCapturingAudio(mMockWebContents1)).thenReturn(true);
        when(mMediaCaptureJniMock.isCapturingVideo(mMockWebContents1)).thenReturn(true);
        Intent staleIntent = captureUpdateIntent(TAB_ID_1, mMockWebContents1);

        when(mMediaCaptureJniMock.isCapturingScreen(mMockWebContents1)).thenReturn(false);
        mService.onStartCommand(staleIntent, /* flags= */ 0, /* startId= */ 1);

        int expectedFgsType =
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CAMERA
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE;
        verify(mMockForegroundServiceUtils)
                .startForeground(
                        eq(mMockService), eq(NOTIFICATION_ID_1), any(), eq(expectedFgsType));
    }

    @Test
    public void testUpdateNotification_crossTypeCaptureSwitch_refreshesToNewMediaType() {
        // 1. Start window capture on Tab 1.
        when(mMediaCaptureJniMock.isCapturingWindow(mMockWebContents1)).thenReturn(true);
        mService.onStartCommand(
                captureUpdateIntent(TAB_ID_1, mMockWebContents1), /* flags= */ 0, /* startId= */ 1);
        clearInvocations(mMockForegroundServiceUtils, mMockService);

        // 2. An in-flight intent carrying [WINDOW_CAPTURE] is queued, and before onStartCommand
        // runs on the UI thread, the user switches shared content from Window to Tab.
        Intent staleWindowIntent = captureUpdateIntent(TAB_ID_1, mMockWebContents1);
        when(mMediaCaptureJniMock.isCapturingWindow(mMockWebContents1)).thenReturn(false);
        when(mMediaCaptureJniMock.isCapturingTab(mMockWebContents1)).thenReturn(true);

        mService.onStartCommand(staleWindowIntent, /* flags= */ 0, /* startId= */ 2);

        int expectedTabFgsType =
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CAMERA
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION;
        verify(mMockForegroundServiceUtils)
                .startForeground(
                        eq(mMockService), eq(NOTIFICATION_ID_1), any(), eq(expectedTabFgsType));
        verify(mMockForegroundServiceUtils, never()).stopForeground(any(), anyInt());
        verify(mMockService, never()).stopSelf(anyInt());
    }

    @Test
    public void testUpdateNotification_staleIntentWhenAllCaptureStopped_stopsSelf() {
        // Intent snapshot contains SCREEN_CAPTURE, but all capture has already stopped on the live
        // WebContents by the time onStartCommand runs.
        when(mMediaCaptureJniMock.isCapturingScreen(mMockWebContents1)).thenReturn(true);
        Intent staleIntent = captureUpdateIntent(TAB_ID_1, mMockWebContents1);

        when(mMediaCaptureJniMock.isCapturingScreen(mMockWebContents1)).thenReturn(false);
        mService.onStartCommand(staleIntent, /* flags= */ 0, /* startId= */ 7);

        verify(mMockForegroundServiceUtils, never())
                .startForeground(any(), anyInt(), any(), anyInt());
        verify(mMockService).stopSelf(7);
    }

    @Test
    public void testUpdateNotification_emptyMediaTypes_skipsJni() {
        ChromeSharedPreferences.getInstance()
                .writeStringSet(
                        ChromePreferenceKeys.MEDIA_WEBRTC_NOTIFICATION_IDS,
                        Set.of(String.valueOf(NOTIFICATION_ID_1)));
        Intent emptyIntent = captureUpdateIntent(TAB_ID_1, /* webContents= */ null);
        clearInvocations(mMediaCaptureJniMock);

        mService.onStartCommand(emptyIntent, /* flags= */ 0, /* startId= */ 8);

        verifyNoInteractions(mMediaCaptureJniMock);
        verify(mMockForegroundServiceUtils, never())
                .startForeground(any(), anyInt(), any(), anyInt());
        verify(mMockService).stopSelf(8);
    }

    @Test
    public void testUpdateNotification_missingTab_stopsSelfWithoutJni() {
        when(mMediaCaptureJniMock.isCapturingScreen(mMockWebContents1)).thenReturn(true);
        Intent intent = captureUpdateIntent(TAB_ID_1, mMockWebContents1);
        clearInvocations(mMediaCaptureJniMock);

        when(mMockTabWindowManager.getTabById(TAB_ID_1)).thenReturn(null);
        mService.onStartCommand(intent, /* flags= */ 0, /* startId= */ 9);

        verifyNoInteractions(mMediaCaptureJniMock);
        verify(mMockForegroundServiceUtils, never())
                .startForeground(any(), anyInt(), any(), anyInt());
        verify(mMockService).stopSelf(9);
    }

    @Test
    public void testUpdateNotification_destroyedWebContents_stopsSelfWithoutJni() {
        when(mMediaCaptureJniMock.isCapturingScreen(mMockWebContents1)).thenReturn(true);
        Intent intent = captureUpdateIntent(TAB_ID_1, mMockWebContents1);
        clearInvocations(mMediaCaptureJniMock);

        when(mMockWebContents1.isDestroyed()).thenReturn(true);
        mService.onStartCommand(intent, /* flags= */ 0, /* startId= */ 10);

        verifyNoInteractions(mMediaCaptureJniMock);
        verify(mMockForegroundServiceUtils, never())
                .startForeground(any(), anyInt(), any(), anyInt());
        verify(mMockService).stopSelf(10);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.MEDIA_CAPTURE_NOTIFICATION_REFRESH_MEDIA_TYPES)
    public void testUpdateNotification_refreshKillSwitchDisabled_usesIntentSnapshotWithoutJni() {
        when(mMediaCaptureJniMock.isCapturingScreen(mMockWebContents1)).thenReturn(true);
        Intent staleIntent = captureUpdateIntent(TAB_ID_1, mMockWebContents1);
        when(mMediaCaptureJniMock.isCapturingScreen(mMockWebContents1)).thenReturn(false);
        clearInvocations(mMediaCaptureJniMock);

        mService.onStartCommand(staleIntent, /* flags= */ 0, /* startId= */ 11);

        verifyNoInteractions(mMediaCaptureJniMock);
        int expectedFgsType =
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CAMERA
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION;
        verify(mMockForegroundServiceUtils)
                .startForeground(
                        eq(mMockService), eq(NOTIFICATION_ID_1), any(), eq(expectedFgsType));
    }

    @Test
    public void testStartOrUpdateForegroundService_cameraMicSecurityExceptionPropagates() {
        when(mMediaCaptureJniMock.isCapturingAudio(mMockWebContents1)).thenReturn(true);
        when(mMediaCaptureJniMock.isCapturingVideo(mMockWebContents1)).thenReturn(true);

        int cameraMicFgsType =
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CAMERA
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE;
        doThrow(new SecurityException("Camera/mic FGS not allowed"))
                .when(mMockForegroundServiceUtils)
                .startForeground(
                        eq(mMockService), eq(NOTIFICATION_ID_1), any(), eq(cameraMicFgsType));

        Intent intent = captureUpdateIntent(TAB_ID_1, mMockWebContents1);
        assertThrows(
                SecurityException.class,
                () -> mService.onStartCommand(intent, /* flags= */ 0, /* startId= */ 1));
        verify(mMockForegroundServiceUtils, never()).stopForeground(any(), anyInt());
    }

    @Test
    public void testStartOrUpdateForegroundService_securityExceptionFallsBackWithoutProjection() {
        when(mMediaCaptureJniMock.isCapturingScreen(mMockWebContents1)).thenReturn(true);
        when(mMediaCaptureJniMock.isCapturingAudio(mMockWebContents1)).thenReturn(true);
        when(mMediaCaptureJniMock.isCapturingVideo(mMockWebContents1)).thenReturn(true);

        int fgsWithProjection =
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CAMERA
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION;
        int fgsWithoutProjection =
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CAMERA
                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE;

        doThrow(new SecurityException("MediaProjection token revoked"))
                .when(mMockForegroundServiceUtils)
                .startForeground(
                        eq(mMockService), eq(NOTIFICATION_ID_1), any(), eq(fgsWithProjection));

        InOrder inOrder = inOrder(mMockForegroundServiceUtils);

        // 1. Indicator lags behind MediaProjection token revocation: 0xE0 throws, falls back to
        // 0xC0.
        Intent firstIntent = captureUpdateIntent(TAB_ID_1, mMockWebContents1);
        mService.onStartCommand(firstIntent, /* flags= */ 0, /* startId= */ 1);

        inOrder.verify(mMockForegroundServiceUtils)
                .startForeground(
                        eq(mMockService), eq(NOTIFICATION_ID_1), any(), eq(fgsWithProjection));
        inOrder.verify(mMockForegroundServiceUtils)
                .startForeground(
                        eq(mMockService), eq(NOTIFICATION_ID_1), any(), eq(fgsWithoutProjection));

        // 2. Native teardown catches up and sends [AUDIO_AND_VIDEO]; FGS stays on 0xC0 without
        // retrying 0xE0.
        when(mMediaCaptureJniMock.isCapturingScreen(mMockWebContents1)).thenReturn(false);
        Intent secondIntent = captureUpdateIntent(TAB_ID_1, mMockWebContents1);
        mService.onStartCommand(secondIntent, /* flags= */ 0, /* startId= */ 2);

        inOrder.verify(mMockForegroundServiceUtils)
                .startForeground(
                        eq(mMockService), eq(NOTIFICATION_ID_1), any(), eq(fgsWithoutProjection));
        inOrder.verifyNoMoreInteractions();
        verify(mMockForegroundServiceUtils, never()).stopForeground(any(), anyInt());
        verify(mMockService, never()).stopSelf(anyInt());
    }

    @Test
    public void testStartOrUpdateForegroundService_securityExceptionStopsActiveFgsWhenNoFallback() {
        // First start an audio call FGS so mStartedForegroundService is true.
        when(mMediaCaptureJniMock.isCapturingAudio(mMockWebContents1)).thenReturn(true);
        mService.onStartCommand(
                captureUpdateIntent(TAB_ID_1, mMockWebContents1), /* flags= */ 0, /* startId= */ 1);
        clearInvocations(mMockForegroundServiceUtils);

        // Only SCREEN_CAPTURE is requested without camera/mic permissions and with a revoked token.
        when(mMediaCaptureJniMock.isCapturingAudio(mMockWebContents1)).thenReturn(false);
        when(mMediaCaptureJniMock.isCapturingScreen(mMockWebContents1)).thenReturn(true);
        when(mMockService.checkPermission(eq(Manifest.permission.CAMERA), anyInt(), anyInt()))
                .thenReturn(PackageManager.PERMISSION_DENIED);
        when(mMockService.checkPermission(eq(Manifest.permission.RECORD_AUDIO), anyInt(), anyInt()))
                .thenReturn(PackageManager.PERMISSION_DENIED);

        doThrow(new SecurityException("MediaProjection token revoked"))
                .when(mMockForegroundServiceUtils)
                .startForeground(
                        eq(mMockService),
                        eq(NOTIFICATION_ID_1),
                        any(),
                        eq(ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION));

        Intent intent = captureUpdateIntent(TAB_ID_1, mMockWebContents1);
        mService.onStartCommand(intent, /* flags= */ 0, /* startId= */ 2);

        verify(mMockForegroundServiceUtils)
                .startForeground(
                        eq(mMockService),
                        eq(NOTIFICATION_ID_1),
                        any(),
                        eq(ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION));
        verify(mMockForegroundServiceUtils)
                .stopForeground(eq(mMockService), eq(Service.STOP_FOREGROUND_REMOVE));
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readStringSet(ChromePreferenceKeys.MEDIA_WEBRTC_NOTIFICATION_IDS, Set.of())
                        .isEmpty());
    }
}
