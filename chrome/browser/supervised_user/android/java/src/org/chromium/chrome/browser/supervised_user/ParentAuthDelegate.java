// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import org.chromium.base.Callback;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/**
 * The correct version of {@link ParentAuthDelegateImpl} will be determined at compile
 * time via build rules.
 */
public interface ParentAuthDelegate {
    /** @see {@link WebsiteParentApproval#isLocalApprovalSupported()} */
    boolean isLocalAuthSupported();

    /** @see {@link WebsiteParentApproval#requestLocalApproval()} */
    void requestLocalAuth(
            WindowAndroid windowAndroid, GURL url, Callback<Boolean> onCompletionCallback);
}
