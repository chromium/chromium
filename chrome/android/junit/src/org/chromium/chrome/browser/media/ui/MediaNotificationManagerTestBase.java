// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.support.v4.media.session.MediaSessionCompat;
import android.view.KeyEvent;

import org.junit.After;
import org.junit.Before;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.AppHooksImpl;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.media.ui.MediaNotificationManager.ListenerService;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.content_public.common.MediaMetadata;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

/**
 * Common test fixtures for MediaNotificationManager JUnit tests.
 */
public class MediaNotificationManagerTestBase {
    private static final int NOTIFICATION_ID = 0;
    static final String NOTIFICATION_GROUP_NAME = "group-name";
    Context mMockContext;
    MockListenerService mService;
    MediaNotificationListener mListener;
    AppHooksImpl mMockAppHooks;
    NotificationUmaTracker mMockUmaTracker;

    MediaNotificationInfo.Builder mMediaNotificationInfoBuilder;

    static class MockMediaNotificationManager extends MediaNotificationManager {
        public MockMediaNotificationManager(NotificationUmaTracker umaTracker, int notificationId) {
            super(umaTracker, notificationId);
        }
    }

    class MockListenerService extends ListenerService {
        @Override
        protected int getNotificationId() {
            return MediaNotificationManagerTestBase.this.getNotificationId();
        }

        @Override
        public int onStartCommand(Intent intent, int flags, int startId) {
            return super.onStartCommand(intent, flags, startId);
        }
    }

    static class MockMediaButtonReceiver extends MediaButtonReceiver {
        @Override
        public Class<?> getServiceClass() {
            return MockListenerService.class;
        }
    }

    @Before
    public void setUp() {
        // For checking the notification presented to NotificationManager.
        assertTrue(RuntimeEnvironment.getApiLevel() >= Build.VERSION_CODES.N);

        ShadowLog.stream = System.out;

        mMockContext = spy(RuntimeEnvironment.application);
        ContextUtils.initApplicationContextForTests(mMockContext);

        mListener = mock(MediaNotificationListener.class);

        MediaNotificationManager.sMapNotificationIdToOptions.put(getNotificationId(),
                new MediaNotificationManager.NotificationOptions(MockListenerService.class,
                        MockMediaButtonReceiver.class, NOTIFICATION_GROUP_NAME));

        mMockUmaTracker = mock(NotificationUmaTracker.class);
        MediaNotificationManager.setManagerForTesting(getNotificationId(),
                spy(new MockMediaNotificationManager(mMockUmaTracker, getNotificationId())));

        mMediaNotificationInfoBuilder =
                new MediaNotificationInfo.Builder()
                        .setMetadata(new MediaMetadata("title", "artist", "album"))
                        .setOrigin("https://example.com")
                        .setListener(mListener)
                        .setId(getNotificationId());

        doNothing().when(getManager()).onServiceStarted(any(ListenerService.class));
        // Robolectric does not have "ShadowMediaSession".
        doAnswer(new Answer() {
            @Override
            public Object answer(InvocationOnMock invocation) {
                MediaSessionCompat mockSession = mock(MediaSessionCompat.class);
                getManager().mMediaSession = mockSession;
                doReturn(null).when(mockSession).getSessionToken();
                return "Created mock media session";
            }
        })
                .when(getManager())
                .updateMediaSession();

        doAnswer(new Answer() {
            @Override
            public Object answer(InvocationOnMock invocation) {
                Intent intent = (Intent) invocation.getArgument(0);
                startService(intent);
                return new ComponentName(mMockContext, MockListenerService.class);
            }
        })
                .when(mMockContext)
                .startService(any(Intent.class));

        mMockAppHooks = mock(AppHooksImpl.class);
        AppHooks.setInstanceForTesting(mMockAppHooks);

        // Init the command line to avoid assertion failure in |SysUtils#isLowEndDevice()|.
        CommandLine.init(null);
        // Init ChromeFeaturesList to avoid assertion failure in
        // MediaNotificationManagerNotificationTest.
        Map<String, Boolean> testFeatures = new HashMap<>();
        testFeatures.put(ChromeFeatureList.HIDE_USER_DATA_FROM_INCOGNITO_NOTIFICATIONS, true);
        ChromeFeatureList.setTestFeatures(testFeatures);
    }

    @After
    public void tearDown() {
        AppHooks.setInstanceForTesting(null);
        MediaNotificationManager.clear(NOTIFICATION_ID);
    }

    MediaNotificationManager getManager() {
        return MediaNotificationManager.getManager(getNotificationId());
    }

    void ensureMediaNotificationInfo() {
        getManager().mMediaNotificationInfo = mMediaNotificationInfoBuilder.build();
    }

    void setUpServiceAndClearInvocations() {
        setUpService();
        clearInvocations(getManager());
        clearInvocations(mService);
        clearInvocations(mMockContext);
        clearInvocations(mMockUmaTracker);
    }

    void setUpService() {
        ensureMediaNotificationInfo();

        Intent intent = getManager().createIntent();
        mMockContext.startService(intent);
    }

    private void startService(Intent intent) {
        ensureService();
        mService.onStartCommand(intent, 0, 0);
    }

    private void ensureService() {
        if (mService != null) return;
        mService = spy(new MockListenerService());

        doAnswer(new Answer() {
            @Override
            public Object answer(InvocationOnMock invocation) {
                mService.onDestroy();
                mService = null;
                return "service stopped";
            }
        })
                .when(mService)
                .stopListenerService();
    }

    Intent createMediaButtonActionIntent(int keyCode) {
        Intent intent = new Intent(Intent.ACTION_MEDIA_BUTTON);
        KeyEvent keyEvent = new KeyEvent(KeyEvent.ACTION_DOWN, keyCode);
        intent.putExtra(Intent.EXTRA_KEY_EVENT, keyEvent);

        return intent;
    }

    Bitmap iconToBitmap(Icon icon) {
        if (icon == null) return null;

        BitmapDrawable drawable = (BitmapDrawable) icon.loadDrawable(mMockContext);
        assert drawable != null;
        return drawable.getBitmap();
    }

    int getNotificationId() {
        return NOTIFICATION_ID;
    }

    void advanceTimeByMillis(int timeMillis) {
        ShadowLooper.idleMainLooper(timeMillis, TimeUnit.MILLISECONDS);
    }
}
