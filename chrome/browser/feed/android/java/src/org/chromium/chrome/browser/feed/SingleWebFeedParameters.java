// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.build.annotations.NullMarked;

/** This class bundles the parameters for the creation of a single web feed. */
@NullMarked
public class SingleWebFeedParameters {
    private final byte[] mWebFeedId;
    private final int mEntryPoint;

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
