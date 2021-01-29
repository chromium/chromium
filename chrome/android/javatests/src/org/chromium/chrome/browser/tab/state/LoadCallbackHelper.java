// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;
import org.chromium.base.test.util.CallbackHelper;

/**
 * Stores a byte array and notifies when said byte array acquisition is complete
 */
public class LoadCallbackHelper extends CallbackHelper {
    private byte[] mRes;

    /**
     * Called when byte array is acquired
     * @param res byte array acquired
     */
    public void notifyCalled(byte[] res) {
        mRes = res;
        notifyCalled();
    }

    /**
     * @return byte array acquired during callback
     */
    public byte[] getRes() {
        return mRes;
    }
}
