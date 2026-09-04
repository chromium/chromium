// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Context;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcel;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.shadows.ShadowBinder;

import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.runtime_library.IWebApkApi;
import org.chromium.webapk.lib.runtime_library.WebApkServiceImpl;

import java.lang.reflect.Field;

/** Tests for WebApkServiceImplWrapper. */
@RunWith(RobolectricTestRunner.class)
public class WebApkServiceImplWrapperTest {
    private static final String FUNCTION_NAME_NOTIFY_NOTIFICATION =
            "TRANSACTION_notifyNotification";

    private static final String FUNCTION_NAME_CANCEL_NOTIFICATION =
            "TRANSACTION_cancelNotification";

    private static final int HOST_BROWSER_UID = 111;

    private MockWebApkServiceImpl mMockWebApkServiceImpl;
    private MockWebApkServiceImplWrapper mWrapper;

    private static class MockWebApkServiceImpl extends WebApkServiceImpl {
        private boolean mIsCalled;

        public MockWebApkServiceImpl(Context context, Bundle bundle) {
            super(context, bundle);
            mIsCalled = false;
        }

        @Override
        public void notifyNotification(
                String platformTag, int platformID, Notification notification) {
            mIsCalled = true;
        }

        @Override
        public void cancelNotification(String platformTag, int platformID) {
            mIsCalled = true;
        }
    }

    private static class MockWebApkServiceImplWrapper extends WebApkServiceImplWrapper {
        private boolean mIsCalled;

        public MockWebApkServiceImplWrapper(Context context, IBinder delegate, int hostBrowserUid) {
            super(context, delegate, hostBrowserUid);
            mIsCalled = false;
        }

        @Override
        public void notifyNotification(
                String platformTag, int platformID, Notification notification) {
            mIsCalled = true;
        }

        @Override
        public void cancelNotification(String platformTag, int platformID) {
            mIsCalled = true;
        }

        @Override
        protected int getApiCode(String name) {
            return getApiCodeHelper(name);
        }
    }

    @Before
    public void setUp() {
        Context context = RuntimeEnvironment.application;
        Bundle bundle = new Bundle();
        bundle.putInt(WebApkServiceFactory.KEY_HOST_BROWSER_UID, HOST_BROWSER_UID);
        mMockWebApkServiceImpl = new MockWebApkServiceImpl(context, bundle);
        mWrapper =
                new MockWebApkServiceImplWrapper(context, mMockWebApkServiceImpl, HOST_BROWSER_UID);
        ShadowBinder.setCallingUid(HOST_BROWSER_UID);
    }

    /** Tests that {@link WebApkServiceImplWrapper#notifyNotification()} is called. */
    @Test
    public void testNotifyNotificationMethodIsCalledOnWrapper() {
        try {
            mWrapper.onTransact(
                    getApiCodeHelper(FUNCTION_NAME_NOTIFY_NOTIFICATION),
                    Mockito.mock(Parcel.class),
                    Mockito.mock(Parcel.class),
                    0);
            Assert.assertTrue(mWrapper.mIsCalled);
            Assert.assertFalse(mMockWebApkServiceImpl.mIsCalled);
        } catch (Exception e) {
            e.printStackTrace();
            Assert.fail();
        }
    }

    /**
     * Tests that {@link WebApkServiceImpl#cancelNotification()} is called, not the
     * WebApkServiceImplWrapper's.
     */
    @Test
    public void testCancelNotificationMethodIsNotCalledOnWrapper() {
        try {
            mWrapper.onTransact(
                    getApiCodeHelper(FUNCTION_NAME_CANCEL_NOTIFICATION),
                    Mockito.mock(Parcel.class),
                    Mockito.mock(Parcel.class),
                    0);
            Assert.assertFalse(mWrapper.mIsCalled);
            Assert.assertTrue(mMockWebApkServiceImpl.mIsCalled);
        } catch (Exception e) {
            e.printStackTrace();
            Assert.fail();
        }
    }

    @Test
    public void testEnsureNotificationChannelExists_importanceDefault() {
        Context context = RuntimeEnvironment.application;
        WebApkServiceImplWrapper wrapper =
                new MockWebApkServiceImplWrapper(context, null, HOST_BROWSER_UID);
        Notification notification =
                new Notification.Builder(context, WebApkConstants.DEFAULT_NOTIFICATION_CHANNEL_ID)
                        .build();
        String channelId = notification.getChannelId();
        Assert.assertNotNull(channelId);
        wrapper.ensureNotificationChannelExists(channelId);
        NotificationManager notificationManager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        NotificationChannel channel =
                notificationManager.getNotificationChannel(
                        WebApkConstants.DEFAULT_NOTIFICATION_CHANNEL_ID);
        Assert.assertNotNull(channel);
        Assert.assertEquals(NotificationManager.IMPORTANCE_DEFAULT, channel.getImportance());
    }

    @Test
    public void
            testEnsureNotificationChannelExists_trustsExplicitHighPriorityChannelIdEvenWithoutPriority() {
        Context context = RuntimeEnvironment.application;
        WebApkServiceImplWrapper wrapper =
                new MockWebApkServiceImplWrapper(context, null, HOST_BROWSER_UID);
        Notification notification =
                new Notification.Builder(
                                context, WebApkConstants.HIGH_PRIORITY_NOTIFICATION_CHANNEL_ID)
                        .build();
        String channelId = notification.getChannelId();
        Assert.assertNotNull(channelId);
        wrapper.ensureNotificationChannelExists(channelId);

        NotificationManager notificationManager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        NotificationChannel channel =
                notificationManager.getNotificationChannel(
                        WebApkConstants.HIGH_PRIORITY_NOTIFICATION_CHANNEL_ID);
        Assert.assertNotNull(channel);
        Assert.assertEquals(NotificationManager.IMPORTANCE_HIGH, channel.getImportance());
    }

    @Test
    @SuppressWarnings("NewApi")
    public void testNotifyNotification_nullChannelIdFallback() {
        Context context = RuntimeEnvironment.application;
        WebApkServiceImplWrapper wrapper =
                new WebApkServiceImplWrapper(context, mMockWebApkServiceImpl, HOST_BROWSER_UID);
        Notification notification = new Notification.Builder(context).build();
        wrapper.notifyNotification("tag", 1, notification);

        NotificationManager notificationManager =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        NotificationChannel channel =
                notificationManager.getNotificationChannel(
                        WebApkConstants.DEFAULT_NOTIFICATION_CHANNEL_ID);
        Assert.assertNotNull(channel);
        Assert.assertEquals(NotificationManager.IMPORTANCE_DEFAULT, channel.getImportance());
    }

    private static int getApiCodeHelper(String name) {
        try {
            Field f = IWebApkApi.Stub.class.getDeclaredField(name);
            f.setAccessible(true);
            return (int) f.get(null);
        } catch (Exception e) {
            e.printStackTrace();
        }

        return -1;
    }
}
