// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.DiscardableReferencePool;

/**
 * A global accessor to the DiscardableReferencePool.
 *
 * This DiscardableReferencePool is created upon first access, and lives forever.
 */
public class GlobalDiscardableReferencePool {
    static final DiscardableReferencePool INSTANCE = new DiscardableReferencePool();

    public static DiscardableReferencePool getReferencePool() {
        return INSTANCE;
    }
}
