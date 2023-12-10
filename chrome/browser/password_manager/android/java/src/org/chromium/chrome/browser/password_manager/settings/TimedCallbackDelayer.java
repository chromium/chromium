// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.os.Handler;

/** An implementation of {@link CallbackDelayer} which runs callbacks after a fixed time delay. */
public final class TimedCallbackDelayer implements CallbackDelayer {
    /** The {@link Handler} used to delay the callbacks. */
    private final Handler mHandler = new Handler();

    /** How long to delay callbacks, in milliseconds. */
    private final long mDelayMillis;

    /**
     * Constructs a delayer which posts callbacks with a fixed time delay.
     * @param delayMillis The common delay of the callbacks, in milliseconds.
     */
    public TimedCallbackDelayer(long delayMillis) {
        assert delayMillis >= 0;
        mDelayMillis = delayMillis;
    }

    @Override
    public void delay(Runnable callback) {
        mHandler.postDelayed(callback, mDelayMillis);
    }
}
