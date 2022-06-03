// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import static org.mockito.ArgumentMatchers.eq;

import android.content.Context;
import android.hardware.input.InputManager;
import android.os.Build;
import android.os.SystemClock;
import android.view.InputEvent;
import android.view.VerifiedKeyEvent;
import android.view.VerifiedMotionEvent;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for InputEventValidator.
 *
 * These tests are not currently compiled and run because Robolectric doesn't support Android R at
 * the time of writing.
 *
 * TODO(https://crbug.com/1198308): Compile, run, and extend this test suite once we can update
 * Robolectric to support R.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.R)
public class InputEventValidatorTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    private InputEvent mInputEvent;

    @Mock
    private VerifiedMotionEvent mMotionEvent;

    @Mock
    private VerifiedKeyEvent mKeyEvent;

    @Mock
    private InputManager mInputManager;

    @Mock
    private Context mContext;

    private InputEventValidator mInputEventValidator = new InputEventValidator();

    @Before
    public void setUp() {
        // Reset by BaseRobolectricTestRunner.
        ContextUtils.initApplicationContextForTests(mContext);
        Mockito.when(mContext.getSystemService(eq(Context.INPUT_SERVICE)))
                .thenReturn(mInputManager);
    }

    @Test
    public void testValidMotionEvent() throws Exception {
        VerifiedMotionEvent mMotionEvent = Mockito.mock(VerifiedMotionEvent.class);
        Mockito.when(mInputManager.verifyInputEvent(eq(mInputEvent))).thenReturn(mMotionEvent);
        Mockito.when(mMotionEvent.getEventTimeNanos()).thenReturn(SystemClock.uptimeMillis());
        Mockito.when(mMotionEvent.getDownTimeNanos()).thenReturn(SystemClock.uptimeMillis() - 1);
        Assert.assertTrue(mInputEventValidator.test(mInputEvent));
        Assert.assertFalse(mInputEventValidator.test(mInputEvent));
    }
}
