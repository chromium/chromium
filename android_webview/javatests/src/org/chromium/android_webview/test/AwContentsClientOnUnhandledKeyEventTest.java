// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.SystemClock;
import android.view.KeyEvent;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests for the WebViewClient.onUnhandledKeyEvent() method. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwContentsClientOnUnhandledKeyEventTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private KeyEventTestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;

    private static class UnhandledKeyEventHelper extends CallbackHelper {
        private final List<KeyEvent> mUnhandledKeyEventList = new ArrayList<>();

        public List<KeyEvent> getUnhandledKeyEventList() {
            return mUnhandledKeyEventList;
        }

        public void clearUnhandledKeyEventList() {
            mUnhandledKeyEventList.clear();
        }

        public void onUnhandledKeyEvent(KeyEvent event) {
            mUnhandledKeyEventList.add(event);
            notifyCalled();
        }
    }

    UnhandledKeyEventHelper mHelper;

    private class KeyEventTestAwContentsClient extends TestAwContentsClient {
        @Override
        public void onUnhandledKeyEvent(KeyEvent event) {
            mHelper.onUnhandledKeyEvent(event);
            super.onUnhandledKeyEvent(event);
        }
    }

    public AwContentsClientOnUnhandledKeyEventTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mContentsClient = new KeyEventTestAwContentsClient();
        mHelper = new UnhandledKeyEventHelper();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "TextInput"})
    public void testUnconsumedKeyEvents() throws Throwable {
        final String data = "<html><head></head><body>Plain page</body>" + "</html>";
        mActivityTestRule.loadDataSync(
                mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(),
                data,
                "text/html",
                false);

        int callCount;

        callCount = mHelper.getCallCount();
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
        mHelper.waitForCallback(callCount, 2);
        assertUnhandledDownAndUp(KeyEvent.KEYCODE_A);

        callCount = mHelper.getCallCount();
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_DEL);
        mHelper.waitForCallback(callCount, 2);
        assertUnhandledDownAndUp(KeyEvent.KEYCODE_DEL);

        callCount = mHelper.getCallCount();
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_ALT_LEFT);
        mHelper.waitForCallback(callCount, 2);
        assertUnhandledDownAndUp(KeyEvent.KEYCODE_ALT_LEFT);
    }

    private boolean dispatchKeyEvent(final KeyEvent event) throws Throwable {
        return ThreadUtils.runOnUiThreadBlocking(() -> mTestContainerView.dispatchKeyEvent(event));
    }

    private void dispatchDownAndUpKeyEvents(final int code) throws Throwable {
        long eventTime = SystemClock.uptimeMillis();
        dispatchKeyEvent(new KeyEvent(eventTime, eventTime, KeyEvent.ACTION_DOWN, code, 0));
        dispatchKeyEvent(new KeyEvent(eventTime, eventTime, KeyEvent.ACTION_UP, code, 0));
    }

    private void assertUnhandledDownAndUp(final int code) {
        List<KeyEvent> list = mHelper.getUnhandledKeyEventList();
        Assert.assertEquals(
                "KeyEvent list: " + Arrays.deepToString(list.toArray()), 2, list.size());
        Assert.assertEquals(KeyEvent.ACTION_DOWN, list.get(0).getAction());
        Assert.assertEquals(code, list.get(0).getKeyCode());
        Assert.assertEquals(KeyEvent.ACTION_UP, list.get(1).getAction());
        Assert.assertEquals(code, list.get(1).getKeyCode());

        mHelper.clearUnhandledKeyEventList();
    }
}
