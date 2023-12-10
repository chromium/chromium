// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

/** This class bundles the parameters for the creation of a single web feed. */
public class SingleWebFeedParameters {
    private byte[] mWebFeedId;
    private int mEntryPoint;

    public SingleWebFeedParameters(byte[] webFeedId, int entryPoint) {
        mWebFeedId = webFeedId;
        mEntryPoint = entryPoint;
    }

    public int getEntryPoint() {
        return mEntryPoint;
    }

    public byte[] getWebFeedId() {
        return mWebFeedId;
    }
}
