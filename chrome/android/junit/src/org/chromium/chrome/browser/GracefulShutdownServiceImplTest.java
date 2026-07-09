// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.Service;
import android.content.Intent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.SplitCompatService;
import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GracefulShutdownServiceImplTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private GracefulShutdownServiceImpl mService;
    private SplitCompatService mMockService;

    @Before
    public void setUp() {
        mService = new GracefulShutdownServiceImpl();
        mMockService = mock(SplitCompatService.class);
        when(mMockService.getString(anyInt())).thenReturn("test_title");
        when(mMockService.getResources())
                .thenReturn(ContextUtils.getApplicationContext().getResources());
        mService.setServiceForTesting(mMockService);
    }

    @Test
    public void testOnStartCommand() {
        Intent intent = new Intent();
        int result = mService.onStartCommand(intent, /* flags= */ 0, /* startId= */ 1);
        assertEquals(Service.START_NOT_STICKY, result);
    }
}
