// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TtcKeyedService}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TtcKeyedServiceTest {
    private static final long NATIVE_PTR = 1;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TtcKeyedService.Natives mNatives;
    @Mock private TtcKeyedService.Observer mObserver;

    private TtcKeyedService mService;

    @Before
    public void setUp() {
        TtcKeyedServiceJni.setInstanceForTesting(mNatives);
        mService = TtcKeyedService.create(NATIVE_PTR);
        mService.addObserver(mObserver);
    }

    @Test
    public void testSetMicMutedLeavesNotifyingToNative() {
        when(mNatives.setMicMuted(NATIVE_PTR, true)).thenReturn(true);

        mService.setMicMuted(true);

        verify(mNatives).setMicMuted(NATIVE_PTR, true);
        // The native side reports the change, whatever made it.
        verify(mObserver, never()).onMicMutedChanged(anyBoolean());
    }

    @Test
    public void testGetSpeaker() {
        when(mNatives.getSpeaker(NATIVE_PTR)).thenReturn(Speaker.MODEL);
        assertEquals(Speaker.MODEL, mService.getSpeaker());
    }

    @Test
    public void testGetSpeakerWithoutNative() {
        TtcKeyedService service = TtcKeyedService.create(0);
        assertEquals(Speaker.IDLE, service.getSpeaker());
    }
}
