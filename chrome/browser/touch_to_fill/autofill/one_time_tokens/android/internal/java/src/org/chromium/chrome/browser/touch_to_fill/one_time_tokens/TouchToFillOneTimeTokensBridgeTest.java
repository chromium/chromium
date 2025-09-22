// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.one_time_tokens;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TouchToFillOneTimeTokensBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private TouchToFillOneTimeTokensBridge mBridge;

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Context mContext;

    @Before
    public void setUp() {
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mContext));
        mBridge = new TouchToFillOneTimeTokensBridge(mWindowAndroid, /* nativeBridge= */ 0L);
    }

    @Test
    public void testBridgeIsCreated() {
        assertNotNull(mBridge);
    }

    @Test
    public void testShowReturnsTrue() {
        assertTrue(mBridge.show("test_token"));
    }
}
