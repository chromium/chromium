// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.os.Build;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Manual;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Manual tests for InputEventValidator. VerifiedInputEvents cannot be generated for tests, so
 * true integration tests have to be manual.
 *
 * You can run these tests with:
 * tools/autotest.py -C out/<dir> InputEventValidatorManualTest -A Manual
 *
 * These tests only work on R+.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class InputEventValidatorManualTest extends DummyUiActivityTestCase {
    private String mPackageName = "com.android.packageName";
    private byte mPackageMac[];

    @Before
    public void setUp() {
        // We could use DisableIf, but given these are only run manually, this makes it easier to
        // realize that the tests don't run pre-R if you run them on the wrong device.
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.R;
    }

    private void setText(String text) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            TextView textView = new TextView(getActivity());
            textView.setText(text);
            getActivity().setContentView(textView);
        });
    }

    private MotionEvent waitForMotionEventWithAction(int action) throws Exception {
        final AtomicReference<MotionEvent> motionEvent = new AtomicReference<MotionEvent>();
        final CountDownLatch countDownLatch = new CountDownLatch(1);
        getActivity().setTouchEventCallback((event) -> {
            if (event.getActionMasked() != action) return;
            motionEvent.set(event);
            countDownLatch.countDown();
        });
        countDownLatch.await();
        return motionEvent.get();
    }

    private KeyEvent waitForKeyEventWithAction(int action) throws Exception {
        final AtomicReference<KeyEvent> keyEvent = new AtomicReference<KeyEvent>();
        final CountDownLatch countDownLatch = new CountDownLatch(1);
        getActivity().setKeyEventCallback((event) -> {
            if (event.getAction() != action) return;
            keyEvent.set(event);
            countDownLatch.countDown();
        });
        countDownLatch.await();
        return keyEvent.get();
    }

    @Test
    @Manual
    public void testValidMotionEvent() throws Exception {
        setText("Tap the screen to continue.");
        MotionEvent validEvent = waitForMotionEventWithAction(MotionEvent.ACTION_UP);

        InputEventValidator validator = new InputEventValidator();
        Assert.assertTrue(validator.test(validEvent));
        Assert.assertFalse(validator.test(validEvent));
    }

    @Test
    @Manual
    public void testInvalidMotionEvent() throws Exception {
        setText("Tap the screen to continue.");
        MotionEvent invalidEvent = waitForMotionEventWithAction(MotionEvent.ACTION_DOWN);

        InputEventValidator validator = new InputEventValidator();
        Assert.assertFalse(validator.test(invalidEvent));
    }

    @Test
    @Manual
    public void testOldMotionEvent() throws Exception {
        setText("Tap the screen then wait 10 seconds.");
        MotionEvent validEvent = waitForMotionEventWithAction(MotionEvent.ACTION_UP);

        Thread.sleep(InputEventValidator.INPUT_EXPIRY_MILLIS + 100);
        InputEventValidator validator = new InputEventValidator();
        Assert.assertFalse(validator.test(validEvent));
    }

    @Test
    @Manual
    public void testValidKeyEvent() throws Exception {
        setText("Press a key to continue.\n eg. adb shell input keyevent 23");
        KeyEvent validEvent = waitForKeyEventWithAction(KeyEvent.ACTION_UP);

        InputEventValidator validator = new InputEventValidator();
        Assert.assertTrue(validator.test(validEvent));
        Assert.assertFalse(validator.test(validEvent));
    }

    @Test
    @Manual
    public void testInvalidKeyEvent() throws Exception {
        setText("Press a key to continue.\n eg. adb shell input keyevent 23");
        KeyEvent validEvent = waitForKeyEventWithAction(KeyEvent.ACTION_DOWN);

        InputEventValidator validator = new InputEventValidator();
        Assert.assertFalse(validator.test(validEvent));
    }

    @Test
    @Manual
    public void testOldKeyEvent() throws Exception {
        setText("Press a key then wait 10 seconds.\n eg. adb shell input keyevent 23");
        KeyEvent validEvent = waitForKeyEventWithAction(KeyEvent.ACTION_UP);

        Thread.sleep(InputEventValidator.INPUT_EXPIRY_MILLIS + 100);
        InputEventValidator validator = new InputEventValidator();
        Assert.assertFalse(validator.test(validEvent));
    }
}
