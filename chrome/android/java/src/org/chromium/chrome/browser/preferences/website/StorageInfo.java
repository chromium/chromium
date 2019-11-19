// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import org.chromium.chrome.browser.preferences.website.WebsitePreferenceBridge.StorageInfoClearedCallback;

import java.io.Serializable;

/**
 * Storage information for a given host URL.
 */
public class StorageInfo implements Serializable {
    private final String mHost;
    private final int mType;
    private final long mSize;

    StorageInfo(String host, int type, long size) {
        mHost = host;
        mType = type;
        mSize = size;
    }

    public String getHost() {
        return mHost;
    }

    public void clear(StorageInfoClearedCallback callback) {
        WebsitePreferenceBridgeJni.get().clearStorageData(mHost, mType, callback);
    }

    public long getSize() {
        return mSize;
    }
}
