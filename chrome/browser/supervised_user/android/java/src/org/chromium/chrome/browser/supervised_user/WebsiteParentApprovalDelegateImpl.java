// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import org.chromium.base.Log;

/**
 * Upstream implementation of {@link WebsiteParentApprovalDelegate}.
 * Downstream targets may provide a different implementation.
 */
public class WebsiteParentApprovalDelegateImpl implements WebsiteParentApprovalDelegate {
    private static final String TAG = "WebParentApplDlgt";

    @Override
    public boolean isLocalApprovalSupported() {
        return false;
    }

    @Override
    public void requestLocalApproval() {
        Log.e(TAG, "Unexpected requestLocalApproval() call.");
        // assert isLocalApprovalSupported();
    }
}
