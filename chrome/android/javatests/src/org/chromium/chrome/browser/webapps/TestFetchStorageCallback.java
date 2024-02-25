// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import org.chromium.base.test.util.CallbackHelper;

/**
 * CallbackHelper subclass which implements WebappRegistry.FetchWebappDataStorageCallback for tests.
 */
public class TestFetchStorageCallback extends CallbackHelper
        implements WebappRegistry.FetchWebappDataStorageCallback {
    protected WebappDataStorage mStorage;

    @Override
    public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
        mStorage = storage;
        notifyCalled();
    }

    public WebappDataStorage getStorage() {
        return mStorage;
    }
}
