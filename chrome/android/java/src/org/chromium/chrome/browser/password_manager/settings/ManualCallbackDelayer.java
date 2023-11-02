// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import java.util.ArrayList;
import java.util.List;

/**
 * An implementation of {@link CallbackDelayer} for tests. It runs callbacks after a manual signal.
 */
public final class ManualCallbackDelayer implements CallbackDelayer {
    /** The callbacks to be run within {@link runCallbacksSynchronously}.*/
    private List<Runnable> mCallbacks = new ArrayList<>();

    @Override
    public void delay(Runnable callback) {
        mCallbacks.add(callback);
    }

    /** Run the callback previously passed into {@link delay}.*/
    public void runCallbacksSynchronously() {
        for (Runnable callback : mCallbacks) callback.run();
        mCallbacks.clear();
    }
}
