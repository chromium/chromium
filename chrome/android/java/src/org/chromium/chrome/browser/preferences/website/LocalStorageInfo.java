// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import org.chromium.chrome.browser.preferences.website.WebsitePreferenceBridge.StorageInfoClearedCallback;

import java.io.Serializable;

/**
 * Local Storage information for a given origin.
 */
public class LocalStorageInfo implements Serializable {
    private final String mOrigin;
    private final long mSize;
    private final boolean mImportantDomain;

    LocalStorageInfo(String origin, long size, boolean importantDomain) {
        mOrigin = origin;
        mSize = size;
        mImportantDomain = importantDomain;
    }

    public String getOrigin() {
        return mOrigin;
    }

    public void clear(StorageInfoClearedCallback callback) {
        // TODO(dullweber): Cookies should call a callback when cleared as well.
        WebsitePreferenceBridgeJni.get().clearCookieData(mOrigin);
        WebsitePreferenceBridgeJni.get().clearLocalStorageData(mOrigin, callback);
    }

    public long getSize() {
        return mSize;
    }

    public boolean isDomainImportant() {
        return mImportantDomain;
    }
}
