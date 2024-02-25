// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import org.chromium.base.test.util.CallbackHelper;

import java.nio.ByteBuffer;

/** Stores a ByteBuffer and notifies when said ByteBuffer acquisition is complete */
public class LoadCallbackHelper extends CallbackHelper {
    private ByteBuffer mRes;

    /**
     * Called when ByteBuffer is acquired
     *
     * @param res ByteBuffer acquired
     */
    public void notifyCalled(ByteBuffer res) {
        mRes = res;
        notifyCalled();
    }

    /**
     * @return ByteBuffer acquired during callback
     */
    public ByteBuffer getRes() {
        return mRes;
    }
}
