// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services.gcm;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.os.Bundle;

import androidx.collection.ArrayMap;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.components.fcm.FcmBridge;
import org.chromium.components.fcm.FcmManager;
import org.chromium.components.gcm_driver.GCMDriver;
import org.chromium.components.gcm_driver.GCMDriverJni;
import org.chromium.components.gcm_driver.GCMMessage;

import java.util.Map;

/** Unit tests for {@link ChromeGcmListenerServiceImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeGcmListenerServiceImplTest {
    private static final String TEST_APP_ID = "com.google.chrome.fcm.test";
    private static final String TEST_MESSAGE_ID = "msg_id_12345";

    @Mock private ChromeBrowserInitializer mMockInitializer;
    @Mock private FcmBridge mMockFcmBridge;
    @Mock private GCMDriver mMockGcmDriver;
    @Mock private GCMDriver.Natives mMockGcmDriverJni;

    private ChromeGcmListenerServiceImpl mService;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        ChromeBrowserInitializer.setForTesting(mMockInitializer);
        FcmBridge.setInstanceForTesting(mMockFcmBridge);
        GCMDriver.setInstanceForTesting(mMockGcmDriver);
        GCMDriverJni.setInstanceForTesting(mMockGcmDriverJni);

        mService = new ChromeGcmListenerServiceImpl();
    }

    @Test
    public void testOnDeletedMessagesForwardsToFcmBridge() {
        mService.onDeletedMessages();
        verify(mMockFcmBridge).onMessagesDeleted();
    }

    @Test
    public void testDispatchMessageToDriverForwardsToFcmBridge() {
        Bundle extras = new Bundle();
        extras.putString("subtype", TEST_APP_ID);
        extras.putString("google.message_id", TEST_MESSAGE_ID);
        extras.putString("key1", "val1");
        byte[] rawData = new byte[] {1, 2, 3};
        extras.putByteArray("rawData", rawData);

        GCMMessage message = new GCMMessage(FcmManager.DEFAULT_PROJECT_ID, extras);

        Map<String, String> expectedData = new ArrayMap<>();
        expectedData.put("subtype", TEST_APP_ID);
        expectedData.put("key1", "val1");

        ChromeGcmListenerServiceImpl.dispatchMessageToDriver(message);

        verify(mMockFcmBridge)
                .onMessageReceived(eq(TEST_MESSAGE_ID), eq(expectedData), eq(rawData));
        verify(mMockGcmDriverJni, never())
                .onMessageReceived(anyLong(), any(), any(), any(), any(), any(), any());
    }

    @Test
    public void testDispatchMessageToDriverDoesNotForwardToFcmBridgeForLegacySenderId() {
        Bundle extras = new Bundle();
        extras.putString("subtype", TEST_APP_ID);
        extras.putString("google.message_id", TEST_MESSAGE_ID);

        GCMMessage message = new GCMMessage("legacy_sender_456", extras);

        ChromeGcmListenerServiceImpl.dispatchMessageToDriver(message);

        verify(mMockFcmBridge, never()).onMessageReceived(any(), any(), any());
        verify(mMockGcmDriverJni)
                .onMessageReceived(
                        anyLong(),
                        eq(TEST_APP_ID),
                        eq("legacy_sender_456"),
                        eq(TEST_MESSAGE_ID),
                        any(),
                        any(),
                        any());
    }
}
