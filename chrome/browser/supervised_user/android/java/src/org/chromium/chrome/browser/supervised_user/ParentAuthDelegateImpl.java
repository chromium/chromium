// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import org.chromium.base.Callback;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * Upstream implementation of {@link ParentAuthDelegate}.
 * Downstream targets may provide a different implementation.
 */
public class ParentAuthDelegateImpl implements ParentAuthDelegate {
    @Override
    public boolean isLocalAuthSupported() {
        return false;
    }

    @Override
    public void requestLocalAuth(
            WindowAndroid windowAndroid, GURL url, Callback<Boolean> onCompletionCallback) {
        throw new UnsupportedOperationException("Local parent auth not supported");
    }
}
