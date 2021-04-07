// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import org.chromium.url.GURL;

/**
 * Interface for classes which need to observe a {@link ContinuousNavigationUserDataImpl}.
 */
public interface ContinuousNavigationUserDataObserver {
    /**
     * Called when the underlying data is no longer valid.
     */
    void onInvalidate();

    /**
     * Called when the underlying data has entirely changed.
     */
    void onUpdate(ContinuousNavigationMetadata metadata);

    /**
     * Called when a new page is loaded that is in the data set.
     */
    void onUrlChanged(GURL currentUrl, boolean onSrp);
}
