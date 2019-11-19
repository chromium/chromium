// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import org.chromium.android_webview.test.TestAwContents.RenderProcessGoneObserver;
import org.chromium.base.test.util.CallbackHelper;

import java.util.concurrent.TimeUnit;

/**
 * The helper class for test to wait for render process gone related events.
 */
public class RenderProcessGoneHelper implements RenderProcessGoneObserver {
    private static final int RENDER_PROCESS_GONE_NOTIFIED_TO_AW_CONTENTS_CLIENT = 1;
    private static final int AW_CONTENTS_DESTROYED = 2;

    private int mState;
    private CallbackHelper mCallbackHelper;
    private Runnable mOnRenderProcessGoneTask;

    public RenderProcessGoneHelper() {
        mCallbackHelper = new CallbackHelper();
    }

    public void waitForRenderProcessGoneNotifiedToAwContentsClient() throws Exception {
        waitForState(RENDER_PROCESS_GONE_NOTIFIED_TO_AW_CONTENTS_CLIENT);
    }

    public void waitForAwContentsDestroyed() throws Exception {
        waitForState(AW_CONTENTS_DESTROYED);
    }

    private void waitForState(int state) throws Exception {
        while (mState < state) {
            mCallbackHelper.waitForCallback(mCallbackHelper.getCallCount(), 1,
                    CallbackHelper.WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        }
        if (mState != state) {
            throw new IllegalStateException("AwContents is in state " + mState);
        }
    }

    @Override
    public void onRenderProcessGoneNotifiedToAwContentsClient() {
        mState = RENDER_PROCESS_GONE_NOTIFIED_TO_AW_CONTENTS_CLIENT;
        mCallbackHelper.notifyCalled();
    }

    @Override
    public void onAwContentsDestroyed() {
        mState = AW_CONTENTS_DESTROYED;
        mCallbackHelper.notifyCalled();
    }

    public void setOnRenderProcessGoneTask(Runnable task) {
        mOnRenderProcessGoneTask = task;
    }
}
