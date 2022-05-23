// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

/**
 * The correct version of {@link WebsiteParentApprovalDelegateImpl} will be determined at compile
 * time via build rules.
 */
public interface WebsiteParentApprovalDelegate {
    /** @see {@link WebsiteParentApproval#isLocalApprovalSupported()} */
    boolean isLocalApprovalSupported();

    /** @see {@link WebsiteParentApproval#requestLocalApproval()} */
    void requestLocalApproval();
}
