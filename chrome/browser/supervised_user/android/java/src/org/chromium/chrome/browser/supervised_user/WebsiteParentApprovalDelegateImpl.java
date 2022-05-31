// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import org.chromium.base.Callback;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * Upstream implementation of {@link WebsiteParentApprovalDelegate}.
 * Downstream targets may provide a different implementation.
 */
public class WebsiteParentApprovalDelegateImpl implements WebsiteParentApprovalDelegate {
    @Override
    public boolean isLocalApprovalSupported() {
        return false;
    }

    @Override
    public void requestLocalApproval(
            WindowAndroid windowAndroid, GURL url, Callback<Boolean> onCompletionCallback) {
        throw new UnsupportedOperationException("Local approval not supported");
    }
}
