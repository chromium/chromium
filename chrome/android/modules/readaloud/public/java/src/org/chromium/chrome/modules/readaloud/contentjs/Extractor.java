// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud.contentjs;

import org.chromium.base.Promise;
import org.chromium.chrome.browser.tab.Tab;

/** Interface for javascript-based extraction of the dateModified tag. */
public interface Extractor {

    /**
     * Retrieve the dalue of the dateModified filed from this tab's webcontents. Returns 0 if not
     * set / in case of an error.
     */
    default Promise<Long> getDateModified(Tab tab) {
        var promise = new Promise<Long>();
        promise.fulfill(0L);
        return promise;
    }
}
