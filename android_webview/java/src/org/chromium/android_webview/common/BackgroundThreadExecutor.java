// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import android.os.Handler;
import android.os.HandlerThread;

import java.util.concurrent.Executor;

/** A helper class which creates a background thread to run tasks on. */
public class BackgroundThreadExecutor implements Executor {
    private final HandlerThread mHandlerThread;
    private final Handler mHandler;

    public BackgroundThreadExecutor(String threadName) {
        mHandlerThread = new HandlerThread(threadName);
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
    }

    @Override
    public void execute(Runnable r) {
        mHandler.post(r);
    }
}
