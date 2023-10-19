// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.view;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_EVENTS_CALLBACK;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_FIRST_TIME;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_SCOPE;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE_NOT_SHOWN;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE_SHOWN;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.PACKAGE_NAME;
import static org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId.WEBAPPS;
import static org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId.WEBAPPS_QUIET;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;

/** Tests for {@link DisclosureNotification}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DisclosureNotificationTest {
    private static final String SCOPE = "https://www.example.com";
    private static final String PACKAGE = "com.example.twa";

    @Mock public ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock public TrustedWebActivityModel.DisclosureEventsCallback mCallback;
    @Mock public NotificationManagerProxy mNotificationManager;

    private TrustedWebActivityModel mModel = new TrustedWebActivityModel();
    private DisclosureNotification mNotification;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mModel.set(DISCLOSURE_EVENTS_CALLBACK, mCallback);
        mModel.set(DISCLOSURE_SCOPE, SCOPE);
        mModel.set(PACKAGE_NAME, PACKAGE);
        mModel.set(DISCLOSURE_FIRST_TIME, true);

        Context context = RuntimeEnvironment.application;
        mNotification =
                new DisclosureNotification(
                        context,
                        context.getResources(),
                        mNotificationManager,
                        mModel,
                        mLifecycleDispatcher);
    }

    @Test
    public void registersForLifecycle() {
        verify(mLifecycleDispatcher).register(eq(mNotification));
    }

    @Test
    public void displaysNotification_highPriority() {
        mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_SHOWN);

        NotificationWrapper notification = verifyAndGetNotification();
        assertEquals(WEBAPPS, notification.getNotification().getChannelId());
    }

    @Test
    public void displaysNotification_lowPriority() {
        mModel.set(DISCLOSURE_FIRST_TIME, false);
        mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_SHOWN);

        NotificationWrapper notification = verifyAndGetNotification();
        assertEquals(WEBAPPS_QUIET, notification.getNotification().getChannelId());
    }

    @Test
    public void dismissesNotification_onStateChange() {
        mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_SHOWN);

        NotificationWrapper notification = verifyAndGetNotification();
        String tag = notification.getMetadata().tag;
        int id = notification.getMetadata().id;

        mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_NOT_SHOWN);
        verify(mNotificationManager).cancel(eq(tag), eq(id));
    }

    @Test
    public void dismissesNotification_onStop() {
        mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_SHOWN);

        NotificationWrapper notification = verifyAndGetNotification();
        String tag = notification.getMetadata().tag;
        int id = notification.getMetadata().id;

        mNotification.onStopWithNative();
        verify(mNotificationManager).cancel(eq(tag), eq(id));
    }

    private NotificationWrapper verifyAndGetNotification() {
        ArgumentCaptor<NotificationWrapper> captor =
                ArgumentCaptor.forClass(NotificationWrapper.class);
        verify(mNotificationManager).notify(captor.capture());

        return captor.getValue();
    }
}
