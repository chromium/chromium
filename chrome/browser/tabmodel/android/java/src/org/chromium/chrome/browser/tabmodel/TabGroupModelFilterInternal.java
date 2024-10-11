// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.lifetime.Destroyable;

/** Package private interface extension of {@link TabGroupModelFilter} */
interface TabGroupModelFilterInternal extends TabGroupModelFilter, Destroyable {
    /**
     * Mark TabState initialized, and TabGroupModelFilter ready to use. This should only be called
     * once, and should only be called by {@link TabGroupModelFilterProvider}.
     */
    /*package*/ void markTabStateInitialized();
}
